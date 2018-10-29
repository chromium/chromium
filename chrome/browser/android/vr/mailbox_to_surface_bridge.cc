// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gl/android/surface_texture.h"

#include <android/native_window_jni.h>

#define VOID_OFFSET(x) reinterpret_cast<void*>(x)
#define SHADER(Src) #Src

namespace {

/* clang-format off */
const char kQuadCopyVertex[] = SHADER(
    precision mediump float;
    attribute vec4 a_Position;
    attribute vec2 a_TexCoordinate;
    varying highp vec2 v_TexCoordinate;
    void main() {
      v_TexCoordinate = a_TexCoordinate;
      gl_Position = a_Position;
    }
);

const char kQuadCopyFragment[] = SHADER(
    precision highp float;
    uniform sampler2D u_Texture;
    varying vec2 v_TexCoordinate;
    void main() {
      gl_FragColor = texture2D(u_Texture, v_TexCoordinate);
    }
);

const float kQuadVertices[] = {
    // x     y    u,   v
    -1.f,  1.f, 0.f, 1.f,
    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
     1.f,  1.f, 1.f, 1.f};
/* clang-format on */

static constexpr int kQuadVerticesSize = sizeof(kQuadVertices);

GLuint CompileShader(gpu::gles2::GLES2Interface* gl,
                     GLenum shader_type,
                     const GLchar* shader_source) {
  GLuint shader_handle = gl->CreateShader(shader_type);
  if (shader_handle != 0) {
    // Pass in the shader source.
    GLint len = strlen(shader_source);
    gl->ShaderSource(shader_handle, 1, &shader_source, &len);
    // Compile the shader.
    gl->CompileShader(shader_handle);
    // Get the compilation status.
    GLint status = 0;
    gl->GetShaderiv(shader_handle, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
      GLint info_log_length = 0;
      gl->GetShaderiv(shader_handle, GL_INFO_LOG_LENGTH, &info_log_length);
      auto str_info_log = std::make_unique<GLchar[]>(info_log_length + 1);
      gl->GetShaderInfoLog(shader_handle, info_log_length, nullptr,
                           str_info_log.get());
      DLOG(ERROR) << "Error compiling shader: " << str_info_log.get();
      gl->DeleteShader(shader_handle);
      shader_handle = 0;
    }
  }

  return shader_handle;
}

GLuint CreateAndLinkProgram(gpu::gles2::GLES2Interface* gl,
                            GLuint vertex_shader_handle,
                            GLuint fragment_shader_handle) {
  GLuint program_handle = gl->CreateProgram();

  if (program_handle != 0) {
    // Bind the vertex shader to the program.
    gl->AttachShader(program_handle, vertex_shader_handle);

    // Bind the fragment shader to the program.
    gl->AttachShader(program_handle, fragment_shader_handle);

    // Link the two shaders together into a program.
    gl->LinkProgram(program_handle);

    // Get the link status.
    GLint link_status = 0;
    gl->GetProgramiv(program_handle, GL_LINK_STATUS, &link_status);

    // If the link failed, delete the program.
    if (link_status == GL_FALSE) {
      GLint info_log_length;
      gl->GetProgramiv(program_handle, GL_INFO_LOG_LENGTH, &info_log_length);

      auto str_info_log = std::make_unique<GLchar[]>(info_log_length + 1);
      gl->GetProgramInfoLog(program_handle, info_log_length, nullptr,
                            str_info_log.get());
      DLOG(ERROR) << "Error compiling program: " << str_info_log.get();
      gl->DeleteProgram(program_handle);
      program_handle = 0;
    }
  }

  return program_handle;
}

GLuint ConsumeTexture(gpu::gles2::GLES2Interface* gl,
                      const gpu::MailboxHolder& mailbox) {
  TRACE_EVENT0("gpu", "MailboxToSurfaceBridge::ConsumeTexture");
  gl->WaitSyncTokenCHROMIUM(mailbox.sync_token.GetConstData());

  return gl->CreateAndConsumeTextureCHROMIUM(mailbox.mailbox.name);
}

}  // namespace

namespace vr {

MailboxToSurfaceBridge::MailboxToSurfaceBridge()
    : constructor_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  DVLOG(1) << __FUNCTION__;
}

MailboxToSurfaceBridge::~MailboxToSurfaceBridge() {
  if (surface_handle_) {
    // Unregister from the surface tracker to avoid a resource leak.
    gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();
    tracker->RemoveSurface(surface_handle_);
  }
  DestroyContext();
  DVLOG(1) << __FUNCTION__;
}

bool MailboxToSurfaceBridge::IsConnected() {
  return context_provider_ && gl_ && context_support_;
}

bool MailboxToSurfaceBridge::IsGpuWorkaroundEnabled(int32_t workaround) {
  DCHECK(IsConnected());

  return context_provider_->GetGpuFeatureInfo().IsWorkaroundEnabled(workaround);
}

void MailboxToSurfaceBridge::OnContextAvailableOnUiThread(
    scoped_refptr<viz::ContextProvider> provider) {
  DVLOG(1) << __FUNCTION__;
  // Must save a reference to the viz::ContextProvider to keep it alive,
  // otherwise the GL context created from it becomes invalid on its
  // destruction.
  context_provider_ = std::move(provider);

  if (on_context_provider_ready_) {
    // We have a custom callback from CreateUnboundContextProvider. Run that.
    // The client is responsible for running BindContextProviderToCurrentThread
    // before use.
    constructor_thread_task_runner_->PostTask(
        FROM_HERE, base::ResetAndReturn(&on_context_provider_ready_));
  } else {
    DCHECK(on_context_bound_);
    constructor_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MailboxToSurfaceBridge::BindContextProviderToCurrentThread,
            base::Unretained(this)));
  }
}

void MailboxToSurfaceBridge::BindContextProviderToCurrentThread() {
  auto result = context_provider_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to init viz::ContextProvider";
    return;
  }

  gl_ = context_provider_->ContextGL();
  context_support_ = context_provider_->ContextSupport();

  if (!gl_) {
    DLOG(ERROR) << "Did not get a GL context";
    return;
  }
  if (!context_support_) {
    DLOG(ERROR) << "Did not get a ContextSupport";
    return;
  }
  InitializeRenderer();

  DVLOG(1) << __FUNCTION__ << ": Context ready";
  if (on_context_bound_) {
    base::ResetAndReturn(&on_context_bound_).Run();
  }
}

void MailboxToSurfaceBridge::CreateSurface(
    gl::SurfaceTexture* surface_texture) {
  ANativeWindow* window = surface_texture->CreateSurface();
  gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();
  ANativeWindow_acquire(window);
  // Skip ANativeWindow_setBuffersGeometry, the default size appears to work.
  surface_ = std::make_unique<gl::ScopedJavaSurface>(surface_texture);
  surface_handle_ =
      tracker->AddSurfaceForNativeWidget(gpu::GpuSurfaceTracker::SurfaceRecord(
          window, surface_->j_surface().obj()));
  // Unregistering happens in the destructor.
  ANativeWindow_release(window);
}

void MailboxToSurfaceBridge::CreateUnboundContextProvider(
    base::OnceClosure callback) {
  on_context_provider_ready_ = std::move(callback);
  DCHECK(!on_context_bound_);
  CreateContextProviderInternal();
}

void MailboxToSurfaceBridge::CreateAndBindContextProvider(
    base::OnceClosure on_bound_callback) {
  on_context_bound_ = std::move(on_bound_callback);
  DCHECK(!on_context_provider_ready_);
  CreateContextProviderInternal();
}

void MailboxToSurfaceBridge::CreateContextProviderInternal() {
  // The callback to run in this thread. It is necessary to keep |surface| alive
  // until the context becomes available. So pass it on to the callback, so that
  // it stays alive, and is destroyed on the same thread once done.
  auto callback =
      base::BindRepeating(&MailboxToSurfaceBridge::OnContextAvailableOnUiThread,
                          weak_ptr_factory_.GetWeakPtr());

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          [](int surface_handle,
             const content::Compositor::ContextProviderCallback& callback) {
            // Our attributes must be compatible with the shared
            // offscreen surface used by virtualized contexts,
            // otherwise mailbox synchronization doesn't work
            // properly - it assumes a shared underlying GL context.
            // See GetCompositorContextAttributes in
            // content/browser/renderer_host/compositor_impl_android.cc
            // and https://crbug.com/699330.
            gpu::ContextCreationAttribs attributes;
            attributes.alpha_size = -1;
            attributes.red_size = 8;
            attributes.green_size = 8;
            attributes.blue_size = 8;
            attributes.stencil_size = 0;
            attributes.depth_size = 0;
            attributes.samples = 0;
            attributes.sample_buffers = 0;
            attributes.bind_generates_resource = false;
            if (base::SysInfo::IsLowEndDevice()) {
              attributes.alpha_size = 0;
              attributes.red_size = 5;
              attributes.green_size = 6;
              attributes.blue_size = 5;
            }
            content::Compositor::CreateContextProvider(
                surface_handle, attributes,
                gpu::SharedMemoryLimits::ForMailboxContext(), callback);
          },
          surface_handle_, callback));
}

void MailboxToSurfaceBridge::ResizeSurface(int width, int height) {
  if (!IsConnected()) {
    // We're not initialized yet, save the requested size for later.
    needs_resize_ = true;
    resize_width_ = width;
    resize_height_ = height;
    return;
  }
  DVLOG(1) << __FUNCTION__ << ": resize Surface to " << width << "x" << height;
  gl_->ResizeCHROMIUM(width, height, 1.f, GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM,
                      false);
  gl_->Viewport(0, 0, width, height);
}

bool MailboxToSurfaceBridge::CopyMailboxToSurfaceAndSwap(
    const gpu::MailboxHolder& mailbox) {
  if (!IsConnected()) {
    // We may not have a context yet, i.e. due to surface initialization
    // being incomplete. This is not an error, but we obviously can't draw
    // yet. TODO(klausw): change the caller to defer this until we are ready.
    return false;
  }

  TRACE_EVENT0("gpu", __FUNCTION__);
  if (needs_resize_) {
    ResizeSurface(resize_width_, resize_height_);
    needs_resize_ = false;
  }

  GLuint sourceTexture = ConsumeTexture(gl_, mailbox);
  DrawQuad(sourceTexture);
  gl_->DeleteTextures(1, &sourceTexture);
  gl_->SwapBuffers(swap_id_++);
  return true;
}

void MailboxToSurfaceBridge::GenSyncToken(gpu::SyncToken* out_sync_token) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());
  gl_->GenSyncTokenCHROMIUM(out_sync_token->GetData());
}

void MailboxToSurfaceBridge::WaitSyncToken(const gpu::SyncToken& sync_token) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());
  gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
}

void MailboxToSurfaceBridge::WaitForClientGpuFence(gfx::GpuFence* gpu_fence) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());
  GLuint id = gl_->CreateClientGpuFenceCHROMIUM(gpu_fence->AsClientGpuFence());
  gl_->WaitGpuFenceCHROMIUM(id);
  gl_->DestroyGpuFenceCHROMIUM(id);
}

void MailboxToSurfaceBridge::CreateGpuFence(
    const gpu::SyncToken& sync_token,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());
  gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  GLuint id = gl_->CreateGpuFenceCHROMIUM();
  context_support_->GetGpuFence(id, std::move(callback));
  gl_->DestroyGpuFenceCHROMIUM(id);
}

uint32_t MailboxToSurfaceBridge::CreateMailboxTexture(gpu::Mailbox* mailbox) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());

  GLuint tex = 0;
  gl_->GenTextures(1, &tex);
  gl_->BindTexture(GL_TEXTURE_2D, tex);
  gl_->ProduceTextureDirectCHROMIUM(tex, mailbox->name);

  return tex;
}

uint32_t MailboxToSurfaceBridge::BindSharedBufferImage(
    gfx::GpuMemoryBuffer* buffer,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    uint32_t texture_id) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());

  auto img = gl_->CreateImageCHROMIUM(buffer->AsClientBuffer(), size.width(),
                                      size.height(), GL_RGBA);

  gl_->BindTexture(GL_TEXTURE_2D, texture_id);
  gl_->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, img);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  return img;
}

void MailboxToSurfaceBridge::UnbindSharedBuffer(GLuint image_id,
                                                GLuint texture_id) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());

  gl_->BindTexture(GL_TEXTURE_2D, texture_id);
  gl_->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  gl_->DestroyImageCHROMIUM(image_id);
}

void MailboxToSurfaceBridge::DestroyContext() {
  gl_ = nullptr;
  context_provider_ = nullptr;
}

void MailboxToSurfaceBridge::InitializeRenderer() {
  GLuint vertex_shader_handle =
      CompileShader(gl_, GL_VERTEX_SHADER, kQuadCopyVertex);
  if (!vertex_shader_handle) {
    DestroyContext();
    return;
  }

  GLuint fragment_shader_handle =
      CompileShader(gl_, GL_FRAGMENT_SHADER, kQuadCopyFragment);
  if (!fragment_shader_handle) {
    DestroyContext();
    return;
  }

  GLuint program_handle =
      CreateAndLinkProgram(gl_, vertex_shader_handle, fragment_shader_handle);
  if (!program_handle) {
    DestroyContext();
    return;
  }

  // Once the program is linked the shader objects are no longer needed
  gl_->DeleteShader(vertex_shader_handle);
  gl_->DeleteShader(fragment_shader_handle);

  GLuint position_handle = gl_->GetAttribLocation(program_handle, "a_Position");
  GLuint texCoord_handle =
      gl_->GetAttribLocation(program_handle, "a_TexCoordinate");
  GLuint texUniform_handle =
      gl_->GetUniformLocation(program_handle, "u_Texture");

  GLuint vertexBuffer = 0;
  gl_->GenBuffers(1, &vertexBuffer);
  gl_->BindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  gl_->BufferData(GL_ARRAY_BUFFER, kQuadVerticesSize, kQuadVertices,
                  GL_STATIC_DRAW);

  // Set state once only, we assume that nobody else modifies GL state in a way
  // that would interfere with our operations.
  gl_->Disable(GL_CULL_FACE);
  gl_->DepthMask(GL_FALSE);
  gl_->Disable(GL_DEPTH_TEST);
  gl_->Disable(GL_SCISSOR_TEST);
  gl_->Disable(GL_BLEND);
  gl_->Disable(GL_POLYGON_OFFSET_FILL);

  // Not using gl_->Viewport, we assume that it defaults to the whole
  // surface and gets updated by ResizeSurface externally as
  // appropriate.

  gl_->UseProgram(program_handle);

  // Bind vertex attributes
  gl_->BindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

  gl_->EnableVertexAttribArray(position_handle);
  gl_->EnableVertexAttribArray(texCoord_handle);

  static constexpr size_t VERTEX_STRIDE = sizeof(float) * 4;
  static constexpr size_t POSITION_ELEMENTS = 2;
  static constexpr size_t TEXCOORD_ELEMENTS = 2;
  static constexpr size_t POSITION_OFFSET = 0;
  static constexpr size_t TEXCOORD_OFFSET = sizeof(float) * 2;

  gl_->VertexAttribPointer(position_handle, POSITION_ELEMENTS, GL_FLOAT, false,
                           VERTEX_STRIDE, VOID_OFFSET(POSITION_OFFSET));
  gl_->VertexAttribPointer(texCoord_handle, TEXCOORD_ELEMENTS, GL_FLOAT, false,
                           VERTEX_STRIDE, VOID_OFFSET(TEXCOORD_OFFSET));

  gl_->ActiveTexture(GL_TEXTURE0);
  gl_->Uniform1i(texUniform_handle, 0);
}

void MailboxToSurfaceBridge::DrawQuad(unsigned int texture_handle) {
  DCHECK(IsConnected());

  // We're redrawing over the entire viewport, but it's generally more
  // efficient on mobile tiling GPUs to clear anyway as a hint that
  // we're done with the old content. TODO(klausw, https://crbug.com/700389):
  // investigate using gl_->DiscardFramebufferEXT here since that's more
  // efficient on desktop, but it would need a capability check since
  // it's not supported on older devices such as Nexus 5X.
  gl_->Clear(GL_COLOR_BUFFER_BIT);

  // Configure texture. This is a 1:1 pixel copy since the surface
  // size is resized to match the source canvas, so we can use
  // GL_NEAREST.
  gl_->BindTexture(GL_TEXTURE_2D, texture_handle);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_->DrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

}  // namespace vr
