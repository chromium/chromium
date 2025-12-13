// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/browser/android/webapps/twa_launch_queue_tab_helper.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "components/webapps/browser/launch_queue/launch_queue.h"
#include "content/public/browser/web_contents.h"
// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebAppLaunchHandler_jni.h"

namespace webapps {
static void JNI_WebAppLaunchHandler_NotifyLaunchQueue(
    JNIEnv* env,
    content::WebContents* web_contents,
    bool start_new_navigation,
    std::string& start_url,
    std::string& package_name,
    std::vector<std::string>& file_uris) {
  webapps::LaunchParams launch_params;
  launch_params.started_new_navigation = start_new_navigation;
  launch_params.app_id = package_name;
  launch_params.target_url = GURL(start_url);
  for (const auto& file_uri : file_uris) {
    launch_params.paths.emplace_back(file_uri);
  }

  auto* helper =
      TwaLaunchQueueTabHelper::GetOrCreateForWebContents(web_contents);
  helper->EnsureLaunchQueue().Enqueue(launch_params);
}

}  // namespace webapps

DEFINE_JNI(WebAppLaunchHandler)
