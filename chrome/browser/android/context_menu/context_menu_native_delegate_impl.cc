// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/context_menu/context_menu_native_delegate_impl.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/download/android/download_controller_base.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "components/embedder_support/android/contextmenu/context_menu_image_format.h"
#include "components/lens/lens_metadata.mojom.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/android/java_bitmap.h"

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

  NOTREACHED_IN_MIGRATION();
  return chrome::mojom::ImageFormat::JPEG;
}

void OnRetrieveImageForShare(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::string& image_extension,
    const std::vector<lens::mojom::LatencyLogPtr>) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_data = base::android::ToJavaByteArray(env, thumbnail_data);
  auto j_extension =
      base::android::ConvertUTF8ToJavaString(env, image_extension);
  base::android::RunObjectCallbackAndroid(
      jcallback, Java_ContextMenuNativeDelegateImpl_createImageCallbackResult(
                     env, j_data, j_extension));
}

void OnRetrieveImageForContextMenu(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::string& filename_extension,
    const std::vector<lens::mojom::LatencyLogPtr>) {
  ContextMenuImageRequest::Start(jcallback, thumbnail_data);
}

}  // namespace

ContextMenuNativeDelegateImpl::ContextMenuNativeDelegateImpl(
    content::WebContents* const web_contents,
    content::ContextMenuParams* const context_menu_params)
    : web_contents_(web_contents), context_menu_params_(context_menu_params) {}

void ContextMenuNativeDelegateImpl::StartDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean jis_link) {
  DownloadControllerBase::Get()->StartContextMenuDownload(
      *context_menu_params_, web_contents_, jis_link);
}

void ContextMenuNativeDelegateImpl::SearchForImage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jrender_frame_host) {
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  if (!render_frame_host)
    return;

  CoreTabHelper::FromWebContents(web_contents_)
      ->SearchByImage(render_frame_host, context_menu_params_->src_url);
}

void ContextMenuNativeDelegateImpl::RetrieveImageForShare(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jrender_frame_host,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px,
    jint jimage_format) {
  RetrieveImageInternal(env, base::BindOnce(&OnRetrieveImageForShare),
                        jrender_frame_host, jcallback, max_width_px,
                        max_height_px, ToChromeMojomImageFormat(jimage_format));
}

void ContextMenuNativeDelegateImpl::RetrieveImageForContextMenu(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jrender_frame_host,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px) {
  // For context menu, Image needs to be PNG for receiving transparency pixels.
  RetrieveImageInternal(env, base::BindOnce(&OnRetrieveImageForContextMenu),
                        jrender_frame_host, jcallback, max_width_px,
                        max_height_px, chrome::mojom::ImageFormat::PNG);
}

void ContextMenuNativeDelegateImpl::RetrieveImageInternal(
    JNIEnv* env,
    ImageRetrieveCallback retrieve_callback,
    const JavaParamRef<jobject>& jrender_frame_host,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px,
    chrome::mojom::ImageFormat image_format) {
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
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

static jlong JNI_ContextMenuNativeDelegateImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jcontext_menu_params) {
  if (jweb_contents.is_null())
    return reinterpret_cast<intptr_t>(nullptr);
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  auto* params =
      context_menu::ContextMenuParamsFromJavaObject(jcontext_menu_params);
  return reinterpret_cast<intptr_t>(
      new ContextMenuNativeDelegateImpl(web_contents, params));
}
