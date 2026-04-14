// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/web_contents_theme_client.h"

#include <map>

#include "base/android/jni_android.h"
#include "base/no_destructor.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/night_mode/jni_headers/WebContentsThemeClient_jni.h"

namespace night_mode {
namespace {

std::map<content::WebContents*, bool>& GetTestingMap() {
  static base::NoDestructor<std::map<content::WebContents*, bool>> testing_map;
  return *testing_map;
}

}  // namespace

WebContentsThemeClient::WebContentsThemeClient(
    content::WebContents* web_contents)
    : content::WebContentsUserData<WebContentsThemeClient>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsThemeClient);

bool WebContentsThemeClient::IsNightModeEnabled() {
  content::WebContents* web_contents = &GetWebContents();
  auto& testing_map = GetTestingMap();
  auto it = testing_map.find(web_contents);
  if (it != testing_map.end()) {
    return it->second;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebContentsThemeClient_isNightModeEnabled(env, &GetWebContents());
}

void WebContentsThemeClient::SetIsNightModeEnabledForTesting(
    content::WebContents* web_contents,
    bool enabled) {
  GetTestingMap()[web_contents] = enabled;
}

bool WebContentsThemeClient::IsForceDarkWebContentEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebContentsThemeClient_isForceDarkWebContentEnabled(env,
                                                                  &GetWebContents());
}

}  // namespace night_mode
