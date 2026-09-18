// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_environment.h"

#include <memory>
#include <string>

#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/test/test_support_android.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/glic/android/test_support_jni_headers/GlicTestEnvironmentAndroid_jni.h"

namespace glic {

// JNI bridge for GlicTestEnvironment on Android.
class GlicTestEnvironmentAndroid {
 public:
  GlicTestEnvironmentAndroid() {
    base::InitAndroidTestPaths(base::FilePath("/sdcard/chromium_tests_root"));
    // Glic guest page runs in an isolated storage partition which blocks
    // cleartext HTTP traffic by default on Android
    // (ERR_CLEARTEXT_NOT_PERMITTED). We run the test server over HTTPS to
    // bypass this restriction.
    https_server_ = std::make_unique<net::test_server::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    env_.SetGlicPagePath("/glic/browser_tests/glic_browser_test_android.html");
    bool success = env_.SetupEmbeddedTestServers(/*http_server=*/nullptr,
                                                 https_server_.get());
    CHECK(success);
  }
  ~GlicTestEnvironmentAndroid() = default;

  std::string GetURL(const std::string& path) {
    return https_server_->GetURL(path).spec();
  }

  GlicInstanceImpl* GetGlicInstance() {
    auto* profile = ProfileManager::GetLastUsedProfileIfLoaded();
    if (!profile) {
      return nullptr;
    }
    return static_cast<GlicInstanceImpl*>(GetOnlyGlicInstance(profile));
  }

  bool IsWebClientConnected() {
    auto* instance = GetGlicInstance();
    return instance && instance->host().IsWebClientConnected();
  }

  content::WebContents* GetGuestWebContents() {
    auto* instance = GetGlicInstance();
    if (!instance) {
      return nullptr;
    }

    return GetGlicGuestWebContents(instance->host().webui_contents());
  }

  void Destroy() {
    // We intentionally leak the test environment and the embedded test server
    // here. Because this test class is annotated with @DoNotBatch, the test
    // runner terminates the process immediately after each test finishes, so
    // the OS will reclaim all resources. Deleting the test server here would
    // trigger a synchronous shutdown which executes a nested RunLoop on the UI
    // thread, which is prohibited and crashes on Android.
  }

 private:
  GlicTestEnvironment env_;
  std::unique_ptr<net::test_server::EmbeddedTestServer> https_server_;
};

static int64_t JNI_GlicTestEnvironmentAndroid_Init() {
  return reinterpret_cast<int64_t>(new GlicTestEnvironmentAndroid());
}

DEFINE_JNI(GlicTestEnvironmentAndroid)

void SetActivityOrientationForTesting(content::WebContents* web_contents,
                                      int orientation) {
  Java_GlicTestEnvironmentAndroid_setActivityOrientation(
      jni_zero::AttachCurrentThread(), web_contents, orientation);
}

}  // namespace glic
