// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/screen_capture/arc_screen_capture_session.h"

#include <utility>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/ui/screen_capture_notification_ui_chromeos.h"
#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/dip_util.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"

namespace arc {

namespace {
// Callback into Android to force screen updates if the queue gets this big.
constexpr size_t kQueueSizeToForceUpdate = 4;
// Drop frames if the queue gets this big.
constexpr size_t kQueueSizeToDropFrames = 8;
// Bytes per pixel, Android returns stride in pixel units, Chrome uses it in
// bytes.
constexpr size_t kBytesPerPixel = 4;

scoped_refptr<viz::ContextProvider> GetContextProvider() {
  return aura::Env::GetInstance()
      ->context_factory()
      ->SharedMainThreadContextProvider();
}

}  // namespace

struct ArcScreenCaptureSession::PendingBuffer {
  PendingBuffer(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_buffer,
                SetOutputBufferCallback callback,
                GLuint texture,
                GLuint image_id)
      : gpu_buffer_(std::move(gpu_buffer)),
        callback_(std::move(callback)),
        texture_(texture),
        image_id_(image_id) {}
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_buffer_;
  SetOutputBufferCallback callback_;
  const GLuint texture_;
  const GLuint image_id_;
};

struct ArcScreenCaptureSession::DesktopTexture {
  DesktopTexture(GLuint texture,
                 gfx::Size size,
                 std::unique_ptr<viz::SingleReleaseCallback> release_callback)
      : texture_(texture),
        size_(size),
        release_callback_(std::move(release_callback)) {}
  const GLuint texture_;
  gfx::Size size_;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback_;
};

// static
mojom::ScreenCaptureSessionPtr ArcScreenCaptureSession::Create(
    mojom::ScreenCaptureSessionNotifierPtr notifier,
    const std::string& display_name,
    content::DesktopMediaID desktop_id,
    const gfx::Size& size,
    bool enable_notification) {
  // This will get cleaned up when the connection error handler is called.
  ArcScreenCaptureSession* session =
      new ArcScreenCaptureSession(std::move(notifier), size);
  mojo::InterfacePtr<mojom::ScreenCaptureSession> result =
      session->Initialize(desktop_id, display_name, enable_notification);
  if (!result)
    delete session;
  return result;
}

ArcScreenCaptureSession::ArcScreenCaptureSession(
    mojom::ScreenCaptureSessionNotifierPtr notifier,
    const gfx::Size& size)
    : binding_(this),
      notifier_(std::move(notifier)),
      size_(size),
      client_native_pixmap_factory_(
          gfx::CreateClientNativePixmapFactoryDmabuf()) {}

mojom::ScreenCaptureSessionPtr ArcScreenCaptureSession::Initialize(
    content::DesktopMediaID desktop_id,
    const std::string& display_name,
    bool enable_notification) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  display_root_window_ =
      content::DesktopMediaID::GetNativeWindowById(desktop_id);
  if (!display_root_window_) {
    LOG(ERROR) << "Unable to find Aura desktop window";
    return nullptr;
  }

  auto context_provider = GetContextProvider();
  gl_helper_ = std::make_unique<viz::GLHelper>(
      context_provider->ContextGL(), context_provider->ContextSupport());

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          display_root_window_);

  gfx::Size desktop_size = display.GetSizeInPixel();

  scaler_ = gl_helper_->CreateScaler(
      viz::GLHelper::ScalerQuality::SCALER_QUALITY_GOOD,
      gfx::Vector2d(desktop_size.width(), desktop_size.height()),
      gfx::Vector2d(size_.width(), size_.height()), false, true, false);

  display_root_window_->GetHost()->compositor()->AddAnimationObserver(this);

  if (enable_notification) {
    // Show the tray notification icon now.
    base::string16 notification_text =
        l10n_util::GetStringFUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT,
                                   base::UTF8ToUTF16(display_name));
    notification_ui_ = ScreenCaptureNotificationUI::Create(notification_text);
    notification_ui_->OnStarted(
        base::BindOnce(&ArcScreenCaptureSession::NotificationStop,
                       weak_ptr_factory_.GetWeakPtr()),
        content::MediaStreamUI::SourceCallback());
  }

  ash::Shell::Get()->display_manager()->inc_screen_capture_active_counter();
  ash::Shell::Get()->UpdateCursorCompositingEnabled();

  mojom::ScreenCaptureSessionPtr interface_ptr;
  binding_.Bind(mojo::MakeRequest(&interface_ptr));
  binding_.set_connection_error_handler(
      base::BindOnce(&ArcScreenCaptureSession::Close, base::Unretained(this)));
  return interface_ptr;
}

void ArcScreenCaptureSession::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delete this;
}

ArcScreenCaptureSession::~ArcScreenCaptureSession() {
  if (!display_root_window_)
    return;

  display_root_window_->GetHost()->compositor()->RemoveAnimationObserver(this);
  ash::Shell::Get()->display_manager()->dec_screen_capture_active_counter();
  ash::Shell::Get()->UpdateCursorCompositingEnabled();
}

void ArcScreenCaptureSession::NotificationStop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Close();
}

void ArcScreenCaptureSession::SetOutputBuffer(
    mojo::ScopedHandle graphics_buffer,
    uint32_t stride,
    SetOutputBufferCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!graphics_buffer.is_valid()) {
    LOG(ERROR) << "Invalid handle passed into SetOutputBuffer";
    std::move(callback).Run();
    return;
  }
  gpu::gles2::GLES2Interface* gl = GetContextProvider()->ContextGL();
  if (!gl) {
    LOG(ERROR) << "Unable to get the GL context";
    std::move(callback).Run();
    return;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  // Dummy modifier.
  handle.native_pixmap_handle.modifier = 0;
  base::PlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(graphics_buffer), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed unwrapping Mojo handle " << mojo_result;
    std::move(callback).Run();
    return;
  }
  handle.native_pixmap_handle.planes.emplace_back(
      stride * kBytesPerPixel, 0, stride * kBytesPerPixel * size_.height(),
      base::ScopedFD(platform_file));
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      gpu::GpuMemoryBufferImplNativePixmap::CreateFromHandle(
          client_native_pixmap_factory_.get(), std::move(handle), size_,
          gfx::BufferFormat::RGBX_8888, gfx::BufferUsage::SCANOUT,
          gpu::GpuMemoryBufferImpl::DestructionCallback());
  if (!gpu_memory_buffer) {
    LOG(ERROR) << "Failed creating GpuMemoryBuffer";
    std::move(callback).Run();
    return;
  }

  GLuint texture;
  gl->GenTextures(1, &texture);
  GLuint id = gl->CreateImageCHROMIUM(gpu_memory_buffer->AsClientBuffer(),
                                      size_.width(), size_.height(), GL_RGB);
  if (!id) {
    LOG(ERROR) << "Failed to allocate backing surface from GpuMemoryBuffer";
    gl->DeleteTextures(1, &texture);
    std::move(callback).Run();
    return;
  }
  gl->BindTexture(GL_TEXTURE_2D, texture);
  gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, id);

  std::unique_ptr<PendingBuffer> pending_buffer =
      std::make_unique<PendingBuffer>(std::move(gpu_memory_buffer),
                                      std::move(callback), texture, id);
  if (texture_queue_.empty()) {
    // Put our GPU buffer into a queue so it can be used on the next callback
    // where we get a desktop texture.
    buffer_queue_.push(std::move(pending_buffer));
  } else {
    // We already have a texture from the animation callbacks, use that for
    // rendering.
    CopyDesktopTextureToGpuBuffer(std::move(texture_queue_.front()),
                                  std::move(pending_buffer));
    texture_queue_.pop();
  }
}

void ArcScreenCaptureSession::QueryCompleted(
    GLuint query_id,
    std::unique_ptr<PendingBuffer> pending_buffer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Notify ARC++ that the buffer is ready.
  std::move(pending_buffer->callback_).Run();

  gpu::gles2::GLES2Interface* gl = GetContextProvider()->ContextGL();
  if (!gl) {
    LOG(ERROR) << "Unable to get the GL context";
    return;
  }

  // Return resources for ARC++ buffer. The GpuMemoryBuffer will go out of
  // scope and be destroyed too.
  gl->BindTexture(GL_TEXTURE_2D, pending_buffer->texture_);
  gl->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, pending_buffer->image_id_);
  gl->DeleteTextures(1, &pending_buffer->texture_);
  gl->DestroyImageCHROMIUM(pending_buffer->image_id_);
  gl->DeleteQueriesEXT(1, &query_id);
}

void ArcScreenCaptureSession::OnDesktopCaptured(
    std::unique_ptr<viz::CopyOutputResult> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  gpu::gles2::GLES2Interface* gl = GetContextProvider()->ContextGL();
  if (!gl) {
    LOG(ERROR) << "Unable to get the GL context";
    return;
  }
  if (result->IsEmpty())
    return;

  // Get the source texture
  gl->WaitSyncTokenCHROMIUM(
      result->GetTextureResult()->sync_token.GetConstData());
  GLuint src_texture = gl->CreateAndConsumeTextureCHROMIUM(
      result->GetTextureResult()->mailbox.name);
  std::unique_ptr<viz::SingleReleaseCallback> release_callback =
      result->TakeTextureOwnership();

  std::unique_ptr<DesktopTexture> desktop_texture =
      std::make_unique<DesktopTexture>(src_texture, result->size(),
                                       std::move(release_callback));
  if (buffer_queue_.empty()) {
    // We don't have a GPU buffer to render to, so put this in a queue to use
    // when we have one.
    texture_queue_.emplace(std::move(desktop_texture));
  } else {
    // Take the first GPU buffer from the queue and render to that.
    CopyDesktopTextureToGpuBuffer(std::move(desktop_texture),
                                  std::move(buffer_queue_.front()));
    buffer_queue_.pop();
  }
}

void ArcScreenCaptureSession::CopyDesktopTextureToGpuBuffer(
    std::unique_ptr<DesktopTexture> desktop_texture,
    std::unique_ptr<PendingBuffer> pending_buffer) {
  auto context_provider = GetContextProvider();
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();

  if (!gl) {
    LOG(ERROR) << "Unable to get the GL context";
    return;
  }
  GLuint query_id;
  gl->GenQueriesEXT(1, &query_id);
  gl->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, query_id);
  scaler_->Scale(desktop_texture->texture_, desktop_texture->size_,
                 gfx::Vector2dF(), pending_buffer->texture_,
                 gfx::Rect(0, 0, size_.width(), size_.height()));
  gl->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);

  // Return CopyOutputResult resources after texture copy happens.
  gl->DeleteTextures(1, &desktop_texture->texture_);
  if (desktop_texture->release_callback_) {
    gpu::SyncToken sync_token;
    gl->GenSyncTokenCHROMIUM(sync_token.GetData());
    desktop_texture->release_callback_->Run(sync_token, false);
  }

  context_provider->ContextSupport()->SignalQuery(
      query_id, base::BindOnce(&ArcScreenCaptureSession::QueryCompleted,
                               weak_ptr_factory_.GetWeakPtr(), query_id,
                               std::move(pending_buffer)));
}

void ArcScreenCaptureSession::OnAnimationStep(base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (texture_queue_.size() >= kQueueSizeToForceUpdate) {
    DVLOG(3) << "AnimationStep callback forcing update due to texture queue "
                "size "
             << texture_queue_.size();
    notifier_->ForceUpdate();
  }
  if (texture_queue_.size() >= kQueueSizeToDropFrames) {
    DVLOG(3) << "AnimationStep callback dropping frame due to texture queue "
                "size "
             << texture_queue_.size();
    return;
  }

  ui::Layer* layer = display_root_window_->layer();
  if (!layer) {
    LOG(ERROR) << "Unable to find layer for the desktop window";
    return;
  }
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
          base::BindOnce(&ArcScreenCaptureSession::OnDesktopCaptured,
                         weak_ptr_factory_.GetWeakPtr()));
  // Clip the requested area to the desktop area. See b/118675936.
  request->set_area(gfx::Rect(display_root_window_->bounds().size()));
  layer->RequestCopyOfOutput(std::move(request));
}

void ArcScreenCaptureSession::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  compositor->RemoveAnimationObserver(this);
}

}  // namespace arc
