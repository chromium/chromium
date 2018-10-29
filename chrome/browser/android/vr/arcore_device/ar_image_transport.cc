// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/ar_image_transport.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/containers/queue.h"
#include "base/trace_event/traced_value.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace device {

namespace {

// Number of shared buffers to use in rotation. Two would be sufficient if
// strictly sequenced, but use an extra one since we currently don't know
// exactly when the Renderer is done with it.
constexpr int kSharedBufferSwapChainSize = 3;

}  // namespace

// TODO(klausw): share this with WebXrPresentationState.
struct SharedFrameBuffer {
  SharedFrameBuffer() = default;
  ~SharedFrameBuffer() = default;

  gfx::Size size;

  std::unique_ptr<gpu::GpuMemoryBufferImplAndroidHardwareBuffer>
      shared_gpu_memory_buffer;

  // Resources in the remote GPU process command buffer context
  std::unique_ptr<gpu::MailboxHolder> mailbox_holder;
  GLuint remote_texture_id = 0;
  GLuint remote_image_id = 0;

  // Resources in the local GL context
  GLuint local_texture_id = 0;
  // This refptr keeps the image alive while processing a frame. That's
  // required because it owns underlying resources and must still be
  // alive when the mailbox texture backed by this image is used.
  scoped_refptr<gl::GLImageEGL> local_glimage;
};

struct SharedFrameBufferSwapChain {
  SharedFrameBufferSwapChain() = default;
  ~SharedFrameBufferSwapChain() = default;

  base::queue<std::unique_ptr<SharedFrameBuffer>> buffers;
  int next_memory_buffer_id = 0;
};

ArImageTransport::ArImageTransport(
    std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge)
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      mailbox_bridge_(std::move(mailbox_bridge)),
      swap_chain_(std::make_unique<SharedFrameBufferSwapChain>()) {}

ArImageTransport::~ArImageTransport() {}

bool ArImageTransport::Initialize() {
  DCHECK(IsOnGlThread());

  mailbox_bridge_->BindContextProviderToCurrentThread();

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  ar_renderer_ = std::make_unique<ArRenderer>();
  glGenTextures(1, &camera_texture_id_arcore_);

  SetupHardwareBuffers();

  return true;
}

GLuint ArImageTransport::GetCameraTextureId() {
  return camera_texture_id_arcore_;
}

void ArImageTransport::ResizeSharedBuffer(const gfx::Size& size,
                                          SharedFrameBuffer* buffer) {
  DCHECK(IsOnGlThread());

  if (buffer->size == size)
    return;

  TRACE_EVENT0("gpu", __FUNCTION__);
  // Unbind previous image (if any).
  if (buffer->remote_image_id) {
    DVLOG(2) << ": UnbindSharedBuffer, remote_image="
             << buffer->remote_image_id;
    mailbox_bridge_->UnbindSharedBuffer(buffer->remote_image_id,
                                        buffer->remote_texture_id);
    buffer->remote_image_id = 0;
  }

  DVLOG(2) << __FUNCTION__ << ": width=" << size.width()
           << " height=" << size.height();
  // Remove reference to previous image (if any).
  buffer->local_glimage = nullptr;

  static constexpr gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  static constexpr gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;

  gfx::GpuMemoryBufferId kBufferId(swap_chain_->next_memory_buffer_id++);
  buffer->shared_gpu_memory_buffer =
      gpu::GpuMemoryBufferImplAndroidHardwareBuffer::Create(
          kBufferId, size, format, usage,
          gpu::GpuMemoryBufferImpl::DestructionCallback());

  buffer->remote_image_id = mailbox_bridge_->BindSharedBufferImage(
      buffer->shared_gpu_memory_buffer.get(), size, format, usage,
      buffer->remote_texture_id);
  DVLOG(2) << ": BindSharedBufferImage, remote_image="
           << buffer->remote_image_id;

  auto img = base::MakeRefCounted<gl::GLImageAHardwareBuffer>(size);

  base::android::ScopedHardwareBufferHandle ahb =
      buffer->shared_gpu_memory_buffer->CloneHandle().android_hardware_buffer;
  bool ret = img->Initialize(ahb.get(), false /* preserved */);
  if (!ret) {
    DLOG(WARNING) << __FUNCTION__ << ": ERROR: failed to initialize image!";
    return;
  }
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer->local_texture_id);
  img->BindTexImage(GL_TEXTURE_EXTERNAL_OES);
  buffer->local_glimage = std::move(img);

  // Save size to avoid resize next time.
  DVLOG(1) << __FUNCTION__ << ": resized to " << size.width() << "x"
           << size.height();
  buffer->size = size;
}

void ArImageTransport::SetupHardwareBuffers() {
  DCHECK(IsOnGlThread());

  glGenFramebuffersEXT(1, &camera_fbo_);

  for (int i = 0; i < kSharedBufferSwapChainSize; ++i) {
    std::unique_ptr<SharedFrameBuffer> buffer =
        std::make_unique<SharedFrameBuffer>();
    // Remote resources
    buffer->mailbox_holder = std::make_unique<gpu::MailboxHolder>();
    buffer->mailbox_holder->texture_target = GL_TEXTURE_2D;
    buffer->remote_texture_id =
        mailbox_bridge_->CreateMailboxTexture(&buffer->mailbox_holder->mailbox);

    // Local resources
    glGenTextures(1, &buffer->local_texture_id);

    // Add to swap chain
    swap_chain_->buffers.push(std::move(buffer));
  }

  glGenFramebuffersEXT(1, &transfer_fbo_);
}

gpu::MailboxHolder ArImageTransport::TransferFrame(
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DCHECK(IsOnGlThread());
  // TODO(klausw): find out when a buffer is actually done being used
  // including by GL so we can know if we are overwriting one.
  DCHECK(swap_chain_->buffers.size() > 0);

  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, transfer_fbo_);

  std::unique_ptr<SharedFrameBuffer> shared_buffer =
      std::move(swap_chain_->buffers.front());
  swap_chain_->buffers.pop();
  ResizeSharedBuffer(frame_size, shared_buffer.get());

  glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_EXTERNAL_OES,
                            shared_buffer->local_texture_id, 0);

  if (!transfer_fbo_completeness_checked_) {
    auto status = glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER);
    DVLOG(1) << __FUNCTION__ << ": framebuffer status=" << std::hex << status;
    DCHECK(status == GL_FRAMEBUFFER_COMPLETE);
    transfer_fbo_completeness_checked_ = true;
  }

  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  // TODO(klausw): see if we can do this one time on initialization. That would
  // be a tiny bit more efficient, but is only safe if ARCore and ArRenderer
  // don't modify these states.
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glViewport(0, 0, frame_size.width(), frame_size.height());

  // Draw the ARCore texture!
  float uv_transform_floats[16];
  uv_transform.matrix().asColMajorf(uv_transform_floats);
  ar_renderer_->Draw(camera_texture_id_arcore_, uv_transform_floats, 0, 0);

  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

  // Make a GpuFence and place it in the GPU stream for sequencing.
  std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
  std::unique_ptr<gfx::GpuFence> gpu_fence = gl_fence->GetGpuFence();
  mailbox_bridge_->WaitForClientGpuFence(gpu_fence.get());
  mailbox_bridge_->GenSyncToken(&shared_buffer->mailbox_holder->sync_token);

  gpu::MailboxHolder rendered_frame_holder = *shared_buffer->mailbox_holder;

  // Done with the shared buffer.
  swap_chain_->buffers.push(std::move(shared_buffer));

  return rendered_frame_holder;
}

bool ArImageTransport::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArImageTransport> ArImageTransportFactory::Create(
    std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge) {
  return std::make_unique<ArImageTransport>(std::move(mailbox_bridge));
}

}  // namespace device
