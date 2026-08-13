// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "chrome/browser/android/webapps/twa_launch_navigation_handle_user_data.h"
#include "chrome/browser/android/webapps/twa_launch_queue_tab_helper.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "components/webapps/browser/launch_queue/launch_queue.h"
#include "content/public/browser/web_contents.h"
// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebAppLaunchHandler_jni.h"

namespace webapps {
static void JNI_WebAppLaunchHandler_PrepareForLaunch(
    JNIEnv* env,
    content::WebContents* web_contents,
    int64_t launch_token,
    const std::string& start_url,
    const std::string& package_name,
    const std::vector<std::string>& file_uris,
    const std::vector<bool>& can_write,
    const std::string& scope_url,
    bool has_speculative_navigation) {
  // The caller must ensure web_contents is not null.
  CHECK(web_contents);
  // Validate URLs at the JNI boundary. If they are invalid (e.g. from a
  // malformed external intent), we ignore the launch gracefully rather than
  // crashing downstream via CHECKs.
  GURL target_gurl(start_url);
  GURL scope_gurl(scope_url);
  if (!target_gurl.is_valid() || !scope_gurl.is_valid()) {
    return;
  }

  webapps::LaunchParams launch_params;
  launch_params.set_started_new_navigation(true);
  launch_params.set_app_id(package_name);
  launch_params.set_target_url(target_gurl);
  launch_params.set_scope(scope_gurl);
  std::vector<base::FilePath> paths;
  paths.reserve(file_uris.size());
  for (const auto& file_uri : file_uris) {
    paths.push_back(base::FilePath(file_uri));
  }
  launch_params.set_paths_with_permissions(std::move(paths), can_write);

  auto* helper =
      TwaLaunchQueueTabHelper::GetOrCreateForWebContents(web_contents);
  helper->PrepareForLaunch(launch_token, std::move(launch_params),
                           has_speculative_navigation);
}

static void JNI_WebAppLaunchHandler_OnLaunchVerified(
    JNIEnv* env,
    content::WebContents* web_contents,
    int64_t launch_token,
    bool success) {
  // The caller must ensure web_contents is not null.
  CHECK(web_contents);
  auto* helper =
      TwaLaunchQueueTabHelper::GetOrCreateForWebContents(web_contents);
  helper->OnLaunchVerified(launch_token, success);
}

static void JNI_WebAppLaunchHandler_EnqueueNonNavigating(
    JNIEnv* env,
    content::WebContents* web_contents,
    const std::string& start_url,
    const std::string& package_name,
    const std::vector<std::string>& file_uris,
    const std::vector<bool>& can_write,
    const std::string& scope_url) {
  // The caller must ensure web_contents is not null.
  CHECK(web_contents);
  // Validate URLs at the JNI boundary. If they are invalid (e.g. from a
  // malformed external intent), we ignore the launch gracefully rather than
  // crashing downstream via CHECKs.
  GURL target_gurl(start_url);
  GURL scope_gurl(scope_url);
  if (!target_gurl.is_valid() || !scope_gurl.is_valid()) {
    return;
  }

  webapps::LaunchParams launch_params;
  launch_params.set_started_new_navigation(false);
  launch_params.set_app_id(package_name);
  launch_params.set_target_url(target_gurl);
  launch_params.set_scope(scope_gurl);
  std::vector<base::FilePath> paths;
  paths.reserve(file_uris.size());
  for (const auto& file_uri : file_uris) {
    paths.push_back(base::FilePath(file_uri));
  }
  launch_params.set_paths_with_permissions(std::move(paths), can_write);

  auto* helper =
      TwaLaunchQueueTabHelper::GetOrCreateForWebContents(web_contents);
  helper->EnqueueNonNavigating(std::move(launch_params));
}

}  // namespace webapps

DEFINE_JNI(WebAppLaunchHandler)
