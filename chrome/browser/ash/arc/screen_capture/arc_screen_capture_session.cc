// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/screen_capture/arc_screen_capture_session.h"

#include <utility>

#include "ash/components/arc/mojom/screen_capture.mojom.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/notifications/screen_capture_notification_ui_ash.h"
#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
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

scoped_refptr<viz::RasterContextProvider> GetContextProvider() {
  return aura::Env::GetInstance()
      ->context_factory()
      ->SharedMainThreadRasterContextProvider();
}

}  // namespace

// Holds ARC++ provided buffer to copy into.
struct ArcScreenCaptureSession::PendingBuffer {
  PendingBuffer(SetOutputBufferCallback callback,
                scoped_refptr<gpu::ClientSharedImage> shared_image)
      : buffer_ready_callback_(std::move(callback)),
        shared_image_(std::move(shared_image)) {}
  SetOutputBufferCallback buffer_ready_callback_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
};

// Holds CopyOutputResult texture.
struct ArcScreenCaptureSession::DesktopTexture {
  DesktopTexture(const gpu::Mailbox& mailbox,
                 viz::ReleaseCallback release_callback)
      : mailbox_(mailbox), release_callback_(std::move(release_callback)) {}
  const gpu::Mailbox mailbox_;
  viz::ReleaseCallback release_callback_;
};

// static
mojo::PendingRemote<mojom::ScreenCaptureSession>
ArcScreenCaptureSession::Create(
    mojo::PendingRemote<mojom::ScreenCaptureSessionNotifier> notifier,
    const std::string& display_name,
    content::DesktopMediaID desktop_id,
    const gfx::Size& size,
    bool enable_notification) {
  // This will get cleaned up when the connection error handler is called.
  ArcScreenCaptureSession* session =
      new ArcScreenCaptureSession(std::move(notifier), size);
  mojo::PendingRemote<mojom::ScreenCaptureSession> result =
      session->Initialize(desktop_id, display_name, enable_notification);
  if (!result) {
    delete session;
  }
  return result;
}

ArcScreenCaptureSession::ArcScreenCaptureSession(
    mojo::PendingRemote<mojom::ScreenCaptureSessionNotifier> notifier,
    const gfx::Size& size)
    : notifier_(std::move(notifier)),
      size_(size),
      client_native_pixmap_factory_(
          gfx::CreateClientNativePixmapFactoryDmabuf()) {}

mojo::PendingRemote<mojom::ScreenCaptureSession>
ArcScreenCaptureSession::Initialize(content::DesktopMediaID desktop_id,
                                    const std::string& display_name,
                                    bool enable_notification) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  display_root_window_ =
      content::DesktopMediaID::GetNativeWindowById(desktop_id);
  if (!display_root_window_) {
    LOG(ERROR) << "Unable to find Aura desktop window";
    return mojo::NullRemote();
  }

  auto context_provider = GetContextProvider();
  context_provider->AddObserver(this);

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          display_root_window_);

  display_root_window_->GetHost()->compositor()->AddAnimationObserver(this);

  if (enable_notification) {
    // Show the tray notification icon now.
    std::u16string notification_text =
        l10n_util::GetStringFUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT,
                                   base::UTF8ToUTF16(display_name));
    notification_ui_ = ScreenCaptureNotificationUI::Create(
        notification_text, /*capturing_web_contents=*/nullptr);
    notification_ui_->OnStarted(
        base::BindOnce(&ArcScreenCaptureSession::NotificationStop,
                       weak_ptr_factory_.GetWeakPtr()),
        content::MediaStreamUI::SourceCallback(),
        std::vector<content::DesktopMediaID>{});
  }

  ash::Shell::Get()->display_manager()->inc_screen_capture_active_counter();
  ash::Shell::Get()->UpdateCursorCompositingEnabled();

  mojo::PendingRemote<mojom::ScreenCaptureSession> remote =
      receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(
      base::BindOnce(&ArcScreenCaptureSession::Close, base::Unretained(this)));
  return remote;
}

void ArcScreenCaptureSession::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delete this;
}

ArcScreenCaptureSession::~ArcScreenCaptureSession() {
  GetContextProvider()->RemoveObserver(this);

  if (!display_root_window_) {
    return;
  }

  display_root_window_->GetHost()->compositor()->RemoveAnimationObserver(this);
  ash::Shell::Get()->display_manager()->dec_screen_capture_active_counter();
  ash::Shell::Get()->UpdateCursorCompositingEnabled();
}

void ArcScreenCaptureSession::NotificationStop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Close();
}

void ArcScreenCaptureSession::SetOutputBufferDeprecated(
    mojo::ScopedHandle graphics_buffer,
    uint32_t stride,
    SetOutputBufferDeprecatedCallback callback) {
  // Defined locally to avoid having to add a dependency on drm_fourcc.h
  constexpr uint64_t DRM_FORMAT_MOD_LINEAR = 0;

  SetOutputBuffer(std::move(graphics_buffer), gfx::BufferFormat::RGBX_8888,
                  DRM_FORMAT_MOD_LINEAR, stride,
                  base::BindOnce(
                      [](base::OnceCallback<void()> callback) {
                        std::move(callback).Run();
                      },
                      std::move(callback)));
}

void ArcScreenCaptureSession::SetOutputBuffer(
    mojo::ScopedHandle graphics_buffer,
    gfx::BufferFormat buffer_format,
    uint64_t buffer_format_modifier,
    uint32_t stride,
    SetOutputBufferCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!graphics_buffer.is_valid()) {
    LOG(ERROR) << "Invalid handle passed into SetOutputBuffer";
    std::move(callback).Run();
    return;
  }
  auto* ri = GetContextProvider()->RasterInterface();
  auto* sii = GetContextProvider()->SharedImageInterface();
  if (!ri || !sii) {
    LOG(ERROR) << "Unable to get the GL context or SharedImageInterface";
    return;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  handle.native_pixmap_handle.modifier = buffer_format_modifier;
  base::ScopedPlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(graphics_buffer), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed unwrapping Mojo handle " << mojo_result;
    std::move(callback).Run();
    return;
  }
  handle.native_pixmap_handle.planes.emplace_back(
      stride * kBytesPerPixel, 0, stride * kBytesPerPixel * size_.height(),
      std::move(platform_file));

  viz::SharedImageFormat si_format = viz::GetSharedImageFormat(buffer_format);

  auto client_shared_image = sii->CreateSharedImage(
      {si_format, size_, gfx::ColorSpace(),
       // NOTE: This SI will be used as the destination of a copy of the desktop
       // texture via the raster interface. Hence, it needs RASTER_WRITE usage.
       // Note that as the browser process raster interface uses
       // RasterImplementation (and not RasterImplementationGLES) as its
       // implementation, GLES2_WRITE usage is not needed.
       gpu::SHARED_IMAGE_USAGE_RASTER_WRITE, "ArcScreenCapture"},
      std::move(handle));
  CHECK(client_shared_image);
  ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

  std::unique_ptr<PendingBuffer> pending_buffer =
      std::make_unique<PendingBuffer>(std::move(callback),
                                      std::move(client_shared_image));
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
    uint32_t query_id,
    std::unique_ptr<DesktopTexture> desktop_texture,
    std::unique_ptr<PendingBuffer> pending_buffer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* ri = GetContextProvider()->RasterInterface();
  auto* sii = GetContextProvider()->SharedImageInterface();
  if (!ri || !sii) {
    LOG(ERROR) << "Unable to get RasterInterface or SharedImageInterface";
    return;
  }

  // Return CopyOutputResult resources after texture copy happens.
  gpu::SyncToken sync_token;
  ri->GenSyncTokenCHROMIUM(sync_token.GetData());
  std::move(desktop_texture->release_callback_).Run(sync_token, false);

  // Notify ARC++ that the buffer is ready.
  std::move(pending_buffer->buffer_ready_callback_).Run();

  ri->DeleteQueriesEXT(1, &query_id);

  // Return resources for ARC++ buffer.
  sii->DestroySharedImage(gpu::SyncToken(),
                          std::move(pending_buffer->shared_image_));
}

void ArcScreenCaptureSession::OnDesktopCaptured(
    std::unique_ptr<viz::CopyOutputResult> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result->IsEmpty() || result->size().width() < size_.width() - 1 ||
      result->size().width() > size_.width() + 1 ||
      result->size().height() < size_.height() - 1 ||
      result->size().height() > size_.height() + 1) {
    // If the display size changed after the CopyOutputRequest was issued the
    // scale ratio might not produce the right sized output. Drop this result
    // since it's not usable. The next CopyOutputRequest to be issued will know
    // the new display size and have the correct scale ratio.
    // Note that result->size() is computed, and so may be a +/- one pixel from
    // the expected size_ value due to rounding and truncation of floating
    // point values. See b/322075216.
    LOG(WARNING)
        << "Ignoring screen capture result due to size mismatch. Expected "
        << size_.ToString() << " but received " << result->size().ToString();
    return;
  }

  DCHECK_EQ(result->format(), viz::CopyOutputResult::Format::RGBA);
  DCHECK_EQ(result->destination(),
            viz::CopyOutputResult::Destination::kNativeTextures);

  auto* ri = GetContextProvider()->RasterInterface();
  if (!ri) {
    LOG(ERROR) << "Unable to get RasterInterface";
    return;
  }
  // Get the source texture - RGBA format is guaranteed to have 1 valid texture
  // if the CopyOutputRequest succeeded:
  gpu::Mailbox result_mailbox = result->GetTextureResult()->mailbox;
  CHECK(!result_mailbox.IsZero());

  viz::CopyOutputResult::ReleaseCallbacks release_callbacks =
      result->TakeTextureOwnership();
  CHECK_EQ(1u, release_callbacks.size());

  std::unique_ptr<DesktopTexture> desktop_texture =
      std::make_unique<DesktopTexture>(result_mailbox,
                                       std::move(release_callbacks[0]));
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
  auto* ri = context_provider->RasterInterface();

  if (!ri) {
    LOG(ERROR) << "Unable to get RasterInterface";
    return;
  }
  uint32_t query_id;
  ri->GenQueriesEXT(1, &query_id);
  ri->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, query_id);
  ri->CopySharedImage(desktop_texture->mailbox_,
                      pending_buffer->shared_image_->mailbox(), GL_TEXTURE_2D,
                      0, 0, 0, 0, size_.width(), size_.height(),
                      /*unpack_flip_y=*/false,
                      /*unpack_premultiply_alpha=*/false);
  ri->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);

  // The query will be signalled after the copy operation has finished on the
  // GPU and ARC++ can safely read from the buffer.
  context_provider->ContextSupport()->SignalQuery(
      query_id,
      base::BindOnce(&ArcScreenCaptureSession::QueryCompleted,
                     weak_ptr_factory_.GetWeakPtr(), query_id,
                     std::move(desktop_texture), std::move(pending_buffer)));
}

void ArcScreenCaptureSession::OnAnimationStep(base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CompositorAnimationObserver::ResetIfActive();
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

  // TODO(kylechar): Ideally this would add `BlitRequest` to `request` so
  // readback happens directly into the final texture rather than making
  // an extra copy in CopyDesktopTextureToGpuBuffer().
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kNativeTextures,
          base::BindOnce(&ArcScreenCaptureSession::OnDesktopCaptured,
                         weak_ptr_factory_.GetWeakPtr()));
  // Clip the requested area to the desktop area. See b/118675936.
  gfx::Size desktop_size = display_root_window_->bounds().size();
  request->set_area(gfx::Rect(desktop_size));

  // Unconditionally set the scaling ratio, even if the two sizes are identical.
  // What may be identical here may not be identical further down when the scale
  // is transformed for the surface. Note that desktop_size is is not in
  // physical pixels, and a scale factor is applied to adjust to them.
  request->SetScaleRatio(
      gfx::Vector2d(desktop_size.width(), desktop_size.height()),
      gfx::Vector2d(size_.width(), size_.height()));

  // Ensure we get the result size we want, and not +/- one pixel due to
  // clamping or rounding.
  request->set_result_selection(gfx::Rect(size_));

  layer->RequestCopyOfOutput(std::move(request));
}

void ArcScreenCaptureSession::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  compositor->RemoveAnimationObserver(this);
}

void ArcScreenCaptureSession::OnContextLost() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Close();
}

}  // namespace arc
