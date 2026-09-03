// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/enterprise_webrtc/enterprise_webrtc_api_observer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_webrtc.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webrtc_diagnostics.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

// Two test extensions back these tests, differing only in what their service
// worker does on startup:
//  - enterprise_webrtc runs its own chrome.test suite (test.js) as soon as it
//    loads; the C++ side only waits for the verdict.
//  - enterprise_webrtc_passive does nothing on its own, so each test drives
//    the API by injecting scripts into its worker via RunInWorker().
class EnterpriseWebrtcApiTestBase : public ExtensionApiTest {
 protected:
  const Extension* LoadPolicyExtension(const base::FilePath& path) {
    ChromeTestExtensionLoader loader(profile());
    loader.set_location(mojom::ManifestLocation::kExternalPolicy);
    loader.set_pack_extension(true);
    return loader.LoadExtension(path).get();
  }

  // Loads the same extension as a normal (non-policy) install, which the
  // permission's "location": "policy" restriction is supposed to reject.
  // The rejected permission produces an install warning, which is the expected
  // outcome here rather than a load failure.
  const Extension* LoadNonPolicyExtension(const base::FilePath& path) {
    ChromeTestExtensionLoader loader(profile());
    loader.set_location(mojom::ManifestLocation::kInternal);
    loader.set_pack_extension(true);
    loader.set_ignore_manifest_warnings(true);
    return loader.LoadExtension(path).get();
  }

  std::string RunInWorker(Profile* target_profile,
                          const std::string& extension_id,
                          const std::string& script) {
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        target_profile, extension_id, script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    return result.is_string() ? result.GetString() : std::string();
  }
};

class EnterpriseWebrtcApiTest : public EnterpriseWebrtcApiTestBase {
 public:
  EnterpriseWebrtcApiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kApiEnterpriseWebrtc);
  }
  ~EnterpriseWebrtcApiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The enterprise_webrtc extension's own JS suite exercises the promise-based
// surface end to end: startCapture resolves and getCaptureStatus then reports
// an active session.
IN_PROC_BROWSER_TEST_F(EnterpriseWebrtcApiTest, JsApiContract) {
  ResultCatcher catcher;
  ASSERT_TRUE(
      LoadPolicyExtension(test_data_dir_.AppendASCII("enterprise_webrtc")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// The regular and incognito instances of a split-mode extension are separate
// clients: starting in one must not block or stop the other. Before capture
// state was scoped per BrowserContext the incognito instance received
// kAlreadyCapturing and silently shared the regular profile's origin filter,
// and ending either side's session ended the other's.
//
// Both sides are driven through the extension API, in each profile's own
// service worker; the manifest declares "incognito": "split" so those are
// separate worker instances. //content is used only to read back the filters.
IN_PROC_BROWSER_TEST_F(EnterpriseWebrtcApiTest, IncognitoSessionIsIndependent) {
  const Extension* extension = LoadPolicyExtension(
      test_data_dir_.AppendASCII("enterprise_webrtc_passive"));
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  ExtensionPrefs::Get(profile())->SetIsIncognitoEnabled(extension_id, true);

  service_worker_test_utils::TestServiceWorkerTaskQueueObserver
      task_queue_observer;
  // Opening an off-the-record tab spawns the process for split-mode extensions.
  content::WebContents* incognito_contents =
      PlatformOpenURLOffTheRecord(profile(), GURL("about:blank"));
  ASSERT_TRUE(incognito_contents);
  Profile* incognito =
      Profile::FromBrowserContext(incognito_contents->GetBrowserContext());
  ASSERT_TRUE(incognito);
  ASSERT_NE(incognito, profile());

  task_queue_observer.WaitForWorkerContextInitialized(extension_id);

  content::WebRtcDiagnostics* diagnostics =
      content::WebRtcDiagnostics::GetInstance();

  // Start in the regular profile, through the API the extension actually uses.
  EXPECT_EQ(RunInWorker(profile(), extension_id,
                        "chrome.enterprise.webrtc.startCapture("
                        "    {origins: ['https://regular.example']})"
                        "  .then(() => chrome.test.sendScriptResult('OK'),"
                        "        (e) => chrome.test.sendScriptResult("
                        "            e.message));"),
            "OK");

  EXPECT_TRUE(diagnostics->IsCapturingForClient(profile(), extension_id));
  // The same extension id in the incognito profile has no session of its own.
  EXPECT_FALSE(diagnostics->IsCapturingForClient(incognito, extension_id));

  // And it can start one, rather than being refused as already capturing.
  EXPECT_EQ(RunInWorker(incognito, extension_id,
                        "chrome.enterprise.webrtc.startCapture("
                        "    {origins: ['https://incognito.example']})"
                        "  .then(() => chrome.test.sendScriptResult('OK'),"
                        "        (e) => chrome.test.sendScriptResult("
                        "            e.message));"),
            "OK");

  // The two sessions keep their own filters.
  auto regular_filter =
      diagnostics->GetFilterOriginsForClient(profile(), extension_id);
  auto incognito_filter =
      diagnostics->GetFilterOriginsForClient(incognito, extension_id);
  ASSERT_TRUE(regular_filter);
  ASSERT_TRUE(incognito_filter);
  ASSERT_EQ(regular_filter->size(), 1u);
  ASSERT_EQ(incognito_filter->size(), 1u);
  EXPECT_EQ(regular_filter->front().host(), "regular.example");
  EXPECT_EQ(incognito_filter->front().host(), "incognito.example");

  // Stopping in incognito leaves the regular profile's session running.
  EXPECT_EQ(diagnostics->StopCaptureForClient(incognito, extension_id),
            content::WebRtcDiagnostics::StopCaptureResult::kSuccess);
  EXPECT_FALSE(diagnostics->IsCapturingForClient(incognito, extension_id));

  EXPECT_EQ(RunInWorker(profile(), extension_id,
                        "chrome.enterprise.webrtc.getCaptureStatus()"
                        "  .then((res) => chrome.test.sendScriptResult("
                        "      String(res.active)));"),
            "true");
  EXPECT_EQ(RunInWorker(incognito, extension_id,
                        "chrome.enterprise.webrtc.getCaptureStatus()"
                        "  .then((res) => chrome.test.sendScriptResult("
                        "      String(res.active)));"),
            "false");
}

// The permission is restricted to policy-installed extensions: a normal
// install of the same extension gets no chrome.enterprise.webrtc at all.
IN_PROC_BROWSER_TEST_F(EnterpriseWebrtcApiTest, NonPolicyInstallHasNoApi) {
  const Extension* extension = LoadNonPolicyExtension(
      test_data_dir_.AppendASCII("enterprise_webrtc_passive"));
  ASSERT_TRUE(extension);

  EXPECT_EQ(RunInWorker(profile(), extension->id(),
                        "chrome.test.sendScriptResult("
                        "    (chrome.enterprise && chrome.enterprise.webrtc)"
                        "        ? 'PRESENT' : 'ABSENT');"),
            "ABSENT");
}

}  // namespace extensions
