// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_EXTERNAL_PRERENDER_HANDLER_ANDROID_H_
#define CHROME_BROWSER_PRERENDER_EXTERNAL_PRERENDER_HANDLER_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace prerender {
class PrerenderHandle;

// A class for handling external prerender requests from other applications.
class ExternalPrerenderHandlerAndroid {
 public:
  ExternalPrerenderHandlerAndroid();

  // Add a prerender with the given url and referrer on the PrerenderManager of
  // the given profile. This is restricted to a single prerender at a time.
  base::android::ScopedJavaLocalRef<jobject> AddPrerender(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& profile,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& referrer,
      jint top,
      jint left,
      jint bottom,
      jint right,
      jboolean forced_prerender);

  // Cancel the prerender associated with the prerender_handle_
  void CancelCurrentPrerender(JNIEnv* env);

  // Whether the PrerenderManager associated with the given profile has any
  // prerenders for the url.
  static bool HasPrerenderedUrl(Profile* profile,
                                GURL url,
                                content::WebContents* web_contents);

  // Whether the PrerenderManager identified by |profile| has recently
  // prefetched the |url|.
  static bool HasRecentlyPrefetchedUrlForTesting(Profile* profile, GURL url);

  // Clears the information about recent prefetches in the PrerenderManager
  // identified by |profile|.
  static bool ClearPrefetchInformationForTesting(Profile* profile);

 private:
  virtual ~ExternalPrerenderHandlerAndroid();
  std::unique_ptr<prerender::PrerenderHandle> prerender_handle_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPrerenderHandlerAndroid);
};

} // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_EXTERNAL_PRERENDER_HANDLER_ANDROID_H_
