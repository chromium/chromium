// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_REGISTRY_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_REGISTRY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

class GURL;

// WebappRegistry is the C++ counterpart of
// org.chromium.chrome.browser.webapp's WebappRegistry in Java.
// All methods in this class which make JNI calls should be declared virtual and
// mocked out in C++ unit tests. The JNI call cannot be made in this environment
// as the Java side will not be initialised.
class WebappRegistry {
 public:
  WebappRegistry() { }

  WebappRegistry(const WebappRegistry&) = delete;
  WebappRegistry& operator=(const WebappRegistry&) = delete;

  virtual ~WebappRegistry() { }

  // Cleans up data stored by web apps on URLs matching |url_filter|.
  virtual void UnregisterWebappsForUrls(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter);

  // Removes history data (last used time and URLs) stored by web apps with
  // URLs matching |url_filter|, whilst leaving other data intact.
  virtual void ClearWebappHistoryForUrls(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter);

  // Returns a std::vector of all origins that have an installed WebAPK.
  virtual std::vector<std::string> GetOriginsWithWebApk();

  // Returns all origins that have a WebAPK or TWA installed.
  virtual std::vector<std::string> GetOriginsWithInstalledApp();

  // Returns a vector of |sync_pb::WebApkSpecifics| with information for each
  // installed WebAPK.
  virtual std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>
  GetWebApkSpecifics() const;

  // Sets an Android Shared Preference bit to indicate that there are WebAPKs
  // that need to be restored from Sync on Chrome's 2nd run.
  static void SetNeedsPwaRestore(bool needs);

  // Gets an Android Shared Preference bit which indicates whether or not there
  // are WebAPKs that need to be restored from Sync on Chrome's 2nd run.
  static bool GetNeedsPwaRestore();
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_WEBAPP_REGISTRY_H_
