// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/api/enterprise_webrtc/enterprise_webrtc_api_observer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_webrtc.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
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
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/origin.h"

namespace extensions {

// Two test extensions back these tests, differing only in what their service
// worker does on startup:
//  - enterprise_webrtc runs its own chrome.test suite (test.js) as soon as it
//    loads; the C++ side only waits for the verdict.
//  - enterprise_webrtc_passive does nothing on its own, so each test drives
//    the API by injecting scripts into its worker via RunInWorker().
class EnterpriseWebrtcApiTestBase : public ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // Lets a single embedded test server serve two origins that differ by more
    // than their port.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // The ID WebRTCInternals gave the peer connection that
  // `render_process_id` made at `origin`, or empty while it has not recorded
  // one. IDs are "<render process id>-<local id>", and local IDs are handed
  // out from 1 per renderer, so a short scan covers every connection these
  // tests create.
  std::string FindPeerConnectionId(int render_process_id,
                                   const url::Origin& origin) {
    for (int local_id = 1; local_id <= 8; ++local_id) {
      const std::string id =
          base::StringPrintf("%d-%d", render_process_id, local_id);
      if (content::WebRtcDiagnostics::GetInstance()->GetOriginForPeerConnection(
              profile(), id) == origin) {
        return id;
      }
    }
    return std::string();
  }

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

// onPeerConnectionAdded originates in WebRTCInternals, which only observes
// connections a renderer creates, and there is no test hook for injecting a
// fake one. These events can only be driven by real RTCPeerConnections in a
// tab.
//
// b.test's connection is created and closed before a.test's. Events reach a
// worker in dispatch order, so if the filter wrongly matched b.test, its
// message would arrive before the a.test message this test waits for.
IN_PROC_BROWSER_TEST_F(EnterpriseWebrtcApiTest, PeerConnectionEvents) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL watched_url =
      embedded_test_server()->GetURL("a.test", "/empty.html");
  const GURL other_url =
      embedded_test_server()->GetURL("b.test", "/empty.html");

  const Extension* extension = LoadPolicyExtension(
      test_data_dir_.AppendASCII("enterprise_webrtc_passive"));
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  ExtensionTestMessageListener event;

  EXPECT_EQ(RunInWorker(profile(), extension_id,
                        content::JsReplace(
                            R"(
                chrome.enterprise.webrtc.onPeerConnectionAdded.addListener(
                    (id, data) => chrome.test.sendMessage(JSON.stringify({
                      id: id,
                      url: data.url,
                      poolSize: JSON.parse(data.rtcConfiguration)
                                    .iceCandidatePoolSize,
                    })));
                chrome.enterprise.webrtc.onPeerConnectionRemoved.addListener(
                    (id) => chrome.test.sendMessage(
                        JSON.stringify({id: id})));
                chrome.enterprise.webrtc.startCapture({origins: [$1]})
                  .then(() => chrome.test.sendScriptResult('OK'),
                        (e) => chrome.test.sendScriptResult(e.message));
                    )",
                            url::Origin::Create(watched_url).Serialize())),
            "OK");

  content::WebContents* tab = GetActiveWebContents();

  // Unlike the ui_test_utils flavor, this fails when the page does not really
  // commit, rather than leaving the listener below to time out.
  ASSERT_TRUE(content::NavigateToURL(tab, other_url));
  ASSERT_TRUE(content::ExecJs(tab, "window.pc = new RTCPeerConnection();"));
  const int other_render_process_id =
      tab->GetPrimaryMainFrame()->GetProcess()->GetID().value();
  std::string other_pc_id;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    other_pc_id = FindPeerConnectionId(other_render_process_id,
                                       url::Origin::Create(other_url));
    return !other_pc_id.empty();
  }));
  ASSERT_TRUE(content::ExecJs(tab, "window.pc.close();"));
  // The entry is dropped as the remove is handled, so its disappearance is the
  // signal that b.test is done producing events.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !content::WebRtcDiagnostics::GetInstance()
                ->GetOriginForPeerConnection(profile(), other_pc_id)
                .has_value();
  }));

  // iceCandidatePoolSize is only serialized when it is non-zero, so asking for
  // one ties the reported configuration to what this page actually did.
  ASSERT_TRUE(content::NavigateToURL(tab, watched_url));
  ASSERT_TRUE(content::ExecJs(
      tab, "window.pc = new RTCPeerConnection({iceCandidatePoolSize: 2});"));

  ASSERT_TRUE(event.WaitUntilSatisfied());
  const base::DictValue added = base::test::ParseJsonDict(event.message());
  const std::string* added_id = added.FindString("id");
  const std::string* added_url = added.FindString("url");
  ASSERT_TRUE(added_id);
  ASSERT_TRUE(added_url);
  EXPECT_EQ(*added_url, watched_url.spec());
  EXPECT_EQ(added.FindInt("poolSize"), 2);

  event.Reset();
  ASSERT_TRUE(content::ExecJs(tab, "window.pc.close();"));
  ASSERT_TRUE(event.WaitUntilSatisfied());
  const base::DictValue removed = base::test::ParseJsonDict(event.message());
  const std::string* removed_id = removed.FindString("id");
  ASSERT_TRUE(removed_id);
  // The ID's format is not part of the IDL contract, but Added and Removed
  // must report the same opaque ID for the same connection.
  EXPECT_EQ(*removed_id, *added_id);
}

// startCapture() with no argument at all is the documented way to ask for
// every origin in the profile, so a connection at an origin no filter ever
// named still has to be reported, on both events.
IN_PROC_BROWSER_TEST_F(EnterpriseWebrtcApiTest,
                       UnfilteredSessionSeesEveryOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("b.test", "/empty.html");

  const Extension* extension = LoadPolicyExtension(
      test_data_dir_.AppendASCII("enterprise_webrtc_passive"));
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  ExtensionTestMessageListener event;

  EXPECT_EQ(RunInWorker(profile(), extension_id,
                        R"(
              chrome.enterprise.webrtc.onPeerConnectionAdded.addListener(
                  (id, data) => chrome.test.sendMessage(
                      'added ' + data.url));
              chrome.enterprise.webrtc.onPeerConnectionRemoved.addListener(
                  (id) => chrome.test.sendMessage('removed ' + id));
              chrome.enterprise.webrtc.startCapture()
                .then(() => chrome.test.sendScriptResult('OK'),
                      (e) => chrome.test.sendScriptResult(e.message));
                  )"),
            "OK");

  content::WebContents* tab = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(tab, url));
  ASSERT_TRUE(content::ExecJs(tab, "window.pc = new RTCPeerConnection();"));

  ASSERT_TRUE(event.WaitUntilSatisfied());
  EXPECT_EQ(event.message(), base::StrCat({"added ", url.spec()}));

  const int render_process_id =
      tab->GetPrimaryMainFrame()->GetProcess()->GetID().value();
  const std::string pc_id =
      FindPeerConnectionId(render_process_id, url::Origin::Create(url));
  ASSERT_FALSE(pc_id.empty());

  event.Reset();
  ASSERT_TRUE(content::ExecJs(tab, "window.pc.close();"));
  ASSERT_TRUE(event.WaitUntilSatisfied());
  EXPECT_EQ(event.message(), base::StrCat({"removed ", pc_id}));
}

// WebRtcDiagnostics reports to every profile for as long as any profile is
// capturing, so a profile with no session of its own still runs its observer
// for every peer connection made in it. Nothing may be dispatched from there.
//
// The incognito profile holds the only session while the first connection is
// created and closed. The regular profile's worker carries both listeners the
// whole time. The event router delivers in dispatch order, so a leak from the
// first connection would arrive before the message this test waits for,
// which belongs to the connection made after the regular profile starts its
// own session. That second session also rules out a listener that was never
// wired up: without it, silence would prove nothing.
IN_PROC_BROWSER_TEST_F(EnterpriseWebrtcApiTest,
                       NoEventsWhileThisProfileHasNoSession) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("a.test", "/empty.html");

  const Extension* extension = LoadPolicyExtension(
      test_data_dir_.AppendASCII("enterprise_webrtc_passive"));
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  ExtensionPrefs::Get(profile())->SetIsIncognitoEnabled(extension_id, true);

  service_worker_test_utils::TestServiceWorkerTaskQueueObserver
      task_queue_observer;
  content::WebContents* incognito_contents =
      PlatformOpenURLOffTheRecord(profile(), GURL("about:blank"));
  ASSERT_TRUE(incognito_contents);
  Profile* incognito =
      Profile::FromBrowserContext(incognito_contents->GetBrowserContext());
  ASSERT_TRUE(incognito);
  ASSERT_NE(incognito, profile());
  task_queue_observer.WaitForWorkerContextInitialized(extension_id);

  // Unfiltered, so nothing below is excluded by an origin that failed to
  // match.
  EXPECT_EQ(RunInWorker(incognito, extension_id,
                        "chrome.enterprise.webrtc.startCapture()"
                        "  .then(() => chrome.test.sendScriptResult('OK'),"
                        "        (e) => chrome.test.sendScriptResult("
                        "            e.message));"),
            "OK");

  ExtensionTestMessageListener event;
  // iceCandidatePoolSize is only serialized when it is non-zero, which makes
  // it a per-connection tag the listener can report back.
  EXPECT_EQ(RunInWorker(profile(), extension_id,
                        R"(
              chrome.enterprise.webrtc.onPeerConnectionAdded.addListener(
                  (id, data) => chrome.test.sendMessage(
                      'added ' + JSON.parse(data.rtcConfiguration)
                          .iceCandidatePoolSize));
              chrome.enterprise.webrtc.onPeerConnectionRemoved.addListener(
                  (id) => chrome.test.sendMessage('removed'));
              chrome.test.sendScriptResult('OK');
                  )"),
            "OK");

  content::WebContents* tab = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(tab, url));
  ASSERT_TRUE(content::ExecJs(
      tab, "window.pc = new RTCPeerConnection({iceCandidatePoolSize: 1});"));
  const int render_process_id =
      tab->GetPrimaryMainFrame()->GetProcess()->GetID().value();
  std::string pc_id;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    pc_id = FindPeerConnectionId(render_process_id, url::Origin::Create(url));
    return !pc_id.empty();
  }));

  ASSERT_TRUE(content::ExecJs(tab, "window.pc.close();"));
  // The metadata entry is erased as the remove is handled, so its
  // disappearance is the signal that both events have been through the
  // regular profile's observer.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !content::WebRtcDiagnostics::GetInstance()
                ->GetOriginForPeerConnection(profile(), pc_id)
                .has_value();
  }));

  EXPECT_EQ(RunInWorker(profile(), extension_id,
                        "chrome.enterprise.webrtc.startCapture()"
                        "  .then(() => chrome.test.sendScriptResult('OK'),"
                        "        (e) => chrome.test.sendScriptResult("
                        "            e.message));"),
            "OK");

  ASSERT_TRUE(content::ExecJs(
      tab, "window.pc = new RTCPeerConnection({iceCandidatePoolSize: 2});"));
  ASSERT_TRUE(event.WaitUntilSatisfied());
  EXPECT_EQ(event.message(), "added 2");
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
