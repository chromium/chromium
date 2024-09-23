// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/functional/callback_forward.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "content/public/browser/render_frame_host.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/content_extraction/android/jni_headers/InnerTextBridge_jni.h"

using ::base::android::JavaParamRef;

namespace {
void OnGotInnerText(
    base::OnceCallback<void(const base::optional_ref<const std::string>)>
        optional_string_callback,
    std::unique_ptr<content_extraction::InnerTextResult> inner_text) {
  if (!optional_string_callback) {
    return;
  }

  if (!inner_text) {
    std::move(optional_string_callback).Run(base::optional_ref<std::string>());
  } else {
    std::move(optional_string_callback).Run(inner_text->inner_text);
  }
}
}  // namespace

void JNI_InnerTextBridge_GetInnerText(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jrender_frame_host,
    const JavaParamRef<jobject>& jcallback) {
  CHECK(jcallback);
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  if (!render_frame_host) {
    base::android::RunOptionalStringCallbackAndroid(
        jcallback, base::optional_ref<std::string>());
    return;
  }

  auto callback =
      base::BindOnce(&base::android::RunOptionalStringCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback));

  content_extraction::GetInnerText(
      *render_frame_host, std::nullopt,
      base::BindOnce(&OnGotInnerText, std::move(callback)));
}
