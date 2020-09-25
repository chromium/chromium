// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/context_menu/chrome_context_menu_populator.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "chrome/android/chrome_jni_headers/ChromeContextMenuPopulator_jni.h"
#include "chrome/browser/download/android/download_controller_base.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/size.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {

class ContextMenuPopulatorImageRequest : public ImageDecoder::ImageRequest {
 public:
  static void Start(const JavaRef<jobject>& jcallback,
                    const std::vector<uint8_t>& thumbnail_data) {
    auto* request = new ContextMenuPopulatorImageRequest(jcallback);
    ImageDecoder::Start(request, thumbnail_data);
  }

 protected:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    base::android::RunObjectCallbackAndroid(
        jcallback_, gfx::ConvertToJavaBitmap(&decoded_image));
    delete this;
  }

  void OnDecodeImageFailed() override {
    base::android::ScopedJavaLocalRef<jobject> j_bitmap;
    base::android::RunObjectCallbackAndroid(jcallback_, j_bitmap);
    delete this;
  }

 private:
  explicit ContextMenuPopulatorImageRequest(const JavaRef<jobject>& jcallback)
      : jcallback_(jcallback) {}

  const base::android::ScopedJavaGlobalRef<jobject> jcallback_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContextMenuPopulatorImageRequest);
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
  return chrome::mojom::ImageFormat::JPEG;
}

void OnRetrieveImageForShare(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const std::string& image_extension) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_data = base::android::ToJavaByteArray(env, thumbnail_data);
  auto j_extension =
      base::android::ConvertUTF8ToJavaString(env, image_extension);
  base::android::RunObjectCallbackAndroid(
      jcallback, Java_ChromeContextMenuPopulator_createImageCallbackResult(
                     env, j_data, j_extension));
}

void OnRetrieveImageForContextMenu(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const std::string& filename_extension) {
  ContextMenuPopulatorImageRequest::Start(jcallback, thumbnail_data);
}

}  // namespace

ChromeContextMenuPopulator::ChromeContextMenuPopulator(
    content::WebContents* web_contents,
    content::ContextMenuParams* context_menu_params,
    content::RenderFrameHost* render_frame_host)
    : web_contents_(web_contents),
      context_menu_params_(context_menu_params),
      render_frame_host_(render_frame_host) {}

void ChromeContextMenuPopulator::OnStartDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean jis_link) {
  std::string headers;
  DownloadControllerBase::Get()->StartContextMenuDownload(
      *context_menu_params_, web_contents_, jis_link, headers);
}

void ChromeContextMenuPopulator::SearchForImage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!render_frame_host_)
    return;

  CoreTabHelper::FromWebContents(web_contents_)
      ->SearchByImageInNewTab(render_frame_host_,
                              context_menu_params_->src_url);
}

void ChromeContextMenuPopulator::RetrieveImageForShare(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px,
    jint j_image_format) {
  RetrieveImageInternal(env, base::BindOnce(&OnRetrieveImageForShare),
                        jcallback, max_width_px, max_height_px,
                        ToChromeMojomImageFormat(j_image_format));
}

void ChromeContextMenuPopulator::RetrieveImageForContextMenu(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px) {
  // For context menu, Image needs to be PNG for receiving transparency pixels.
  RetrieveImageInternal(env, base::BindOnce(&OnRetrieveImageForContextMenu),
                        jcallback, max_width_px, max_height_px,
                        chrome::mojom::ImageFormat::PNG);
}

void ChromeContextMenuPopulator::RetrieveImageInternal(
    JNIEnv* env,
    ImageRetrieveCallback retrieve_callback,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px,
    chrome::mojom::ImageFormat image_format) {
  if (!render_frame_host_)
    return;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  // Bind the InterfacePtr into the callback so that it's kept alive
  // until there's either a connection error or a response.
  auto* thumbnail_capturer_proxy = chrome_render_frame.get();
  thumbnail_capturer_proxy->RequestImageForContextNode(
      max_width_px * max_height_px, gfx::Size(max_width_px, max_height_px),
      image_format,
      base::BindOnce(
          std::move(retrieve_callback), base::Passed(&chrome_render_frame),
          base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static jlong JNI_ChromeContextMenuPopulator_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jcontext_menu_params,
    const JavaParamRef<jobject>& jrender_frame_host) {
  if (jweb_contents.is_null())
    return reinterpret_cast<intptr_t>(nullptr);
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  auto* params =
      context_menu::ContextMenuParamsFromJavaObject(jcontext_menu_params);
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  return reinterpret_cast<intptr_t>(
      new ChromeContextMenuPopulator(web_contents, params, render_frame_host));
}
