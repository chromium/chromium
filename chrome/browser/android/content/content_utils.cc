// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/android/content/jni_headers/ContentUtils_jni.h"

static std::string JNI_ContentUtils_GetBrowserUserAgent(JNIEnv* env) {
  return embedder_support::GetUserAgent();
}

static void JNI_ContentUtils_SetUserAgentOverride(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    jboolean j_override_in_new_tabs) {
  constexpr char kLinuxInfoStr[] = "X11; Linux x86_64";

  const blink::UserAgentMetadata metadata =
      embedder_support::GetUserAgentMetadata();

  blink::UserAgentOverride spoofed_ua;
  spoofed_ua.ua_string_override =
      embedder_support::BuildUserAgentFromOSAndProduct(
          kLinuxInfoStr, embedder_support::GetProductAndVersion());
  spoofed_ua.ua_metadata_override = metadata;
  spoofed_ua.ua_metadata_override->platform = "Linux";
  spoofed_ua.ua_metadata_override->platform_version =
      std::string();  // match content::GetOSVersion(false) on Linux
  spoofed_ua.ua_metadata_override->model = std::string();
  spoofed_ua.ua_metadata_override->mobile = false;
  spoofed_ua.ua_metadata_override->form_factors =
      embedder_support::GetFormFactorsClientHint(metadata, /*is_mobile=*/false);
  // Match the above "CpuInfo" string, which is also the most common Linux
  // CPU architecture and bitness.`
  spoofed_ua.ua_metadata_override->architecture = "x86";
  spoofed_ua.ua_metadata_override->bitness = "64";
  spoofed_ua.ua_metadata_override->wow64 = false;

  content::WebContents::FromJavaWebContents(jweb_contents)
      ->SetUserAgentOverride(spoofed_ua, j_override_in_new_tabs);
}
