// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UPDATE_DATA_FETCHER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UPDATE_DATA_FETCHER_H_

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_icons_hasher.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class WebContents;
}

class GURL;

namespace webapps {
struct InstallableData;
}

// WebApkUpdateDataFetcher is the C++ counterpart of
// org.chromium.chrome.browser's WebApkUpdateDataFetcher in Java. It is created
// via a JNI (Initialize) call and MUST BE DESTROYED via Destroy().
class WebApkUpdateDataFetcher : public content::WebContentsObserver {
 public:
  WebApkUpdateDataFetcher(JNIEnv* env,
                          jobject obj,
                          const GURL& start_url,
                          const GURL& scope,
                          const GURL& web_manifest_url,
                          const GURL& web_manifest_id);

  WebApkUpdateDataFetcher(const WebApkUpdateDataFetcher&) = delete;
  WebApkUpdateDataFetcher& operator=(const WebApkUpdateDataFetcher&) = delete;

  // Replaces the WebContents that is being observed.
  void ReplaceWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& java_web_contents);

  // Called by the Java counterpart to destroy its native half.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called by the Java counterpart to start checking web manifest changes.
  void Start(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             const base::android::JavaParamRef<jobject>& java_web_contents);

 private:
  ~WebApkUpdateDataFetcher() override;

  // content::WebContentsObserver:
  void DidStopLoading() override;

  // Fetches the installable data.
  void FetchInstallableData();

  // Called once the installable data has been fetched.
  void OnDidGetInstallableData(
      const webapps::InstallableData& installable_data);

  // Called with the computed Murmur2 hashes for the icons.
  void OnGotIconMurmur2Hashes(
      std::map<GURL, std::unique_ptr<webapps::WebappIcon>> icons);

  // Called when a page has no Web Manifest or the Web Manifest is not WebAPK
  // compatible.
  void OnWebManifestNotWebApkCompatible();

  // Points to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // The WebAPK's current start url. Used for recording UMA.
  const GURL start_url_;

  // The detector will only fetch the URL within the scope of the WebAPK.
  const GURL scope_;

  // The WebAPK's Web Manifest URL that the detector is looking for.
  const GURL web_manifest_url_;

  // The WebAPK's Web Manifest ID that the detector is looking for.
  const GURL web_manifest_id_;

  // The URL for which the installable data is being fetched / was last fetched.
  GURL last_fetched_url_;

  // Downloaded data for |web_manifest_url_|.
  webapps::ShortcutInfo info_;
  SkBitmap primary_icon_;
  bool is_primary_icon_maskable_;

  SkBitmap splash_icon_;

  // Helper for downloading WebAPK icons and compute Murmur2 hash of the
  // downloaded images.
  std::unique_ptr<webapps::WebApkIconsHasher> icon_hasher_;

  base::WeakPtrFactory<WebApkUpdateDataFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UPDATE_DATA_FETCHER_H_
