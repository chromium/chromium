// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/context_menu/context_menu_native_delegate_impl.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "build/android_buildflags.h"
#include "chrome/browser/download/android/download_controller_base.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "components/embedder_support/android/contextmenu/context_menu_image_format.h"
#include "components/lens/lens_metadata.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom.h"
#include "ui/gfx/android/java_bitmap.h"

#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
#include "chrome/browser/devtools/devtools_window.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/contextmenu/jni_headers/ContextMenuNativeDelegateImpl_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {

class ContextMenuImageRequest : public ImageDecoder::ImageRequest {
 public:
  ContextMenuImageRequest() = delete;
  ContextMenuImageRequest(const ContextMenuImageRequest&) = delete;
  ContextMenuImageRequest& operator=(const ContextMenuImageRequest&) = delete;

  static void Start(const JavaRef<jobject>& jcallback,
                    const std::vector<uint8_t>& thumbnail_data) {
    auto* request = new ContextMenuImageRequest(jcallback);
    ImageDecoder::Start(request, thumbnail_data);
  }

 protected:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    base::android::RunObjectCallbackAndroid(
        jcallback_, gfx::ConvertToJavaBitmap(decoded_image));
    delete this;
  }

  void OnDecodeImageFailed() override {
    base::android::ScopedJavaLocalRef<jobject> j_bitmap;
    base::android::RunObjectCallbackAndroid(jcallback_, j_bitmap);
    delete this;
  }

 private:
  explicit ContextMenuImageRequest(const JavaRef<jobject>& jcallback)
      : jcallback_(jcallback) {}

  const base::android::ScopedJavaGlobalRef<jobject> jcallback_;
};

chrome::mojom::ImageFormat ToChromeMojomImageFormat(int image_format) {
  auto format = static_cast<ContextMenuImageFormat>(image_format);
  switch (format) {
    case ContextMenuImageFormat::JPEG:
      return chrome::mojom::ImageFormat::JPEG;
    case ContextMenuImageFormat::PNG:
      return chrome::mojom::ImageFormat::PNG;
    case ContextMenuImageFormat::ORIGINAL:
      return chrome::mojom::ImageFormat::ORIGINAL;
  }

  NOTREACHED();
}

// TODO(crbug.com/b/455400488) - Remove image_extension once the Java side is
// updated to only use mime_type.
std::string GetExtensionFromMimeType(const std::string& mime_type) {
  if (mime_type == "image/png") {
    return ".png";
  }
  if (mime_type == "image/jpeg") {
    return ".jpg";
  }
  if (mime_type == "image/webp") {
    return ".webp";
  }
  if (mime_type == "image/gif") {
    return ".gif";
  }
  return "";
}

void OnRetrieveImageForShare(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::string& mime_type,
    const std::vector<lens::mojom::LatencyLogPtr>) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_data = base::android::ToJavaByteArray(env, thumbnail_data);
  std::string image_extension = GetExtensionFromMimeType(mime_type);
  base::android::RunObjectCallbackAndroid(
      jcallback, Java_ContextMenuNativeDelegateImpl_createImageCallbackResult(
                     env, j_data, image_extension));
}

void OnRetrieveImageForContextMenu(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::string& mime_type,
    const std::vector<lens::mojom::LatencyLogPtr>) {
  ContextMenuImageRequest::Start(jcallback, thumbnail_data);
}

}  // namespace

ContextMenuNativeDelegateImpl::ContextMenuNativeDelegateImpl(
    content::WebContents* const web_contents,
    content::ContextMenuParams* const context_menu_params)
    : web_contents_(web_contents), context_menu_params_(context_menu_params) {}

void ContextMenuNativeDelegateImpl::StartDownload(JNIEnv* env,
                                                  const GURL& url,
                                                  jboolean jis_media) {
  DownloadControllerBase::Get()->StartContextMenuDownload(
      url, *context_menu_params_, web_contents_, jis_media);
}

void ContextMenuNativeDelegateImpl::SearchForImage(
    JNIEnv* env,
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return;

  CoreTabHelper::FromWebContents(web_contents_)
      ->SearchByImage(render_frame_host, context_menu_params_->src_url);
}

void ContextMenuNativeDelegateImpl::InspectElement(
    JNIEnv* env,
    content::RenderFrameHost* render_frame_host,
    jint x,
    jint y) {
#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
  if (!render_frame_host) {
    return;
  }
  DevToolsWindow::InspectElement(render_frame_host, x, y);
#else
  NOTREACHED();
#endif
}

void ContextMenuNativeDelegateImpl::RetrieveImageForShare(
    JNIEnv* env,
    content::RenderFrameHost* render_frame_host,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px,
    jint jimage_format) {
  RetrieveImageInternal(env, base::BindOnce(&OnRetrieveImageForShare),
                        render_frame_host, jcallback, max_width_px,
                        max_height_px, ToChromeMojomImageFormat(jimage_format));
}

void ContextMenuNativeDelegateImpl::RetrieveImageForContextMenu(
    JNIEnv* env,
    content::RenderFrameHost* render_frame_host,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px) {
  // For context menu, Image needs to be PNG for receiving transparency pixels.
  RetrieveImageInternal(env, base::BindOnce(&OnRetrieveImageForContextMenu),
                        render_frame_host, jcallback, max_width_px,
                        max_height_px, chrome::mojom::ImageFormat::PNG);
}

void ContextMenuNativeDelegateImpl::RetrieveImageInternal(
    JNIEnv* env,
    ImageRetrieveCallback retrieve_callback,
    content::RenderFrameHost* render_frame_host,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px,
    chrome::mojom::ImageFormat image_format) {
  if (!render_frame_host)
    return;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  // Bind the InterfacePtr into the callback so that it's kept alive
  // until there's either a connection error or a response.
  auto* thumbnail_capturer_proxy = chrome_render_frame.get();
  thumbnail_capturer_proxy->RequestImageForContextNode(
      max_width_px * max_height_px, gfx::Size(max_width_px, max_height_px),
      image_format, chrome::mojom::kDefaultQuality,
      base::BindOnce(
          std::move(retrieve_callback), std::move(chrome_render_frame),
          base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

void ContextMenuNativeDelegateImpl::SetPictureInPicture(
    JNIEnv* env,
    content::RenderFrameHost* render_frame_host,
    jboolean enter_pip) {
  if (!render_frame_host) {
    return;
  }

  render_frame_host->ExecuteMediaPlayerActionAtLocation(
      gfx::Point(context_menu_params_->x, context_menu_params_->y),
      blink::mojom::MediaPlayerAction(
          blink::mojom::MediaPlayerActionType::kPictureInPicture, enter_pip));
}

static jlong JNI_ContextMenuNativeDelegateImpl_Init(
    JNIEnv* env,
    content::WebContents* web_contents,
    const JavaParamRef<jobject>& jcontext_menu_params) {
  DCHECK(web_contents);
  auto* params =
      context_menu::ContextMenuParamsFromJavaObject(jcontext_menu_params);
  return reinterpret_cast<intptr_t>(
      new ContextMenuNativeDelegateImpl(web_contents, params));
}

DEFINE_JNI(ContextMenuNativeDelegateImpl)
