// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXT_MENU_CONTEXT_MENU_NATIVE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ANDROID_CONTEXT_MENU_CONTEXT_MENU_NATIVE_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class WebContents;
}

class ContextMenuNativeDelegateImpl {
 public:
  explicit ContextMenuNativeDelegateImpl(
      content::WebContents* const web_contents,
      content::ContextMenuParams* const context_menu_params);

  void RetrieveImageForContextMenu(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px);
  void RetrieveImageForShare(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px,
      jint j_image_type);
  void StartDownload(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jboolean jis_link);
  void SearchForImage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jrender_frame_host);

 protected:
  using ImageRetrieveCallback = base::OnceCallback<void(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame_ptr,
      const base::android::JavaRef<jobject>& jcallback,
      const std::vector<uint8_t>& thumbnail_data,
      const gfx::Size& original_size,
      const gfx::Size& downscaled_size,
      const std::string& image_extension,
      const std::vector<lens::mojom::LatencyLogPtr>)>;

 private:
  void RetrieveImageInternal(
      JNIEnv* env,
      ImageRetrieveCallback retrieve_callback,
      const base::android::JavaParamRef<jobject>& jrender_frame_host,
      const base::android::JavaParamRef<jobject>& jcallback,
      jint max_width_px,
      jint max_height_px,
      chrome::mojom::ImageFormat image_format);

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<content::ContextMenuParams> context_menu_params_;
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXT_MENU_CONTEXT_MENU_NATIVE_DELEGATE_IMPL_H_
