// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PRECONNECTOR_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PRECONNECTOR_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/preconnect_manager.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace android_webview {

// Holds a content::PreconnectManager, owning it for the lifetime of the Profile
// and exposing via the Java AwPreconnector (which this class also owns).
// Lifetime: Profile
class AwPreconnector : public content::PreconnectManager::Delegate {
 public:
  explicit AwPreconnector(content::BrowserContext* browser_context);
  ~AwPreconnector() override;

  AwPreconnector(const AwPreconnector&) = delete;
  AwPreconnector& operator=(const AwPreconnector&) = delete;

  // Preconnects to the given URL. Returns false if the URL is invalid.
  bool Preconnect(JNIEnv* env, const GURL& url);

  base::android::ScopedJavaLocalRef<jobject> GetJavaAwPreconnector();

  // PreconnectManager::Delegate:
  void PreconnectInitiated(const GURL& url,
                           const GURL& preconnect_url) override;
  void PreconnectFinished(
      std::unique_ptr<content::PreconnectStats> stats) override;

  bool IsPreconnectEnabled() override;

 private:
  content::PreconnectManager& GetPreconnectManager();

  const raw_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<content::PreconnectManager> preconnect_manager_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  base::WeakPtrFactory<AwPreconnector> weak_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PRECONNECTOR_H_
