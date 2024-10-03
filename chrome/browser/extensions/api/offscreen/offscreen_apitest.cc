// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/offscreen/audio_lifetime_enforcer.h"
#include "extensions/browser/api/offscreen/offscreen_api.h"
#include "extensions/browser/api/offscreen/offscreen_document_manager.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "content/public/common/content_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {

// A helper class to wait until a given WebContents is audible or inaudible.
// TODO(devlin): Put this somewhere common? //content/public/test/?
class AudioWaiter : public content::WebContentsObserver {
 public:
  explicit AudioWaiter(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}

  void WaitForAudible() {
    if (web_contents()->IsCurrentlyAudible()) {
      return;
    }
    expected_state_ = true;
    run_loop_.Run();
  }

  void WaitForInaudible() {
    if (!web_contents()->IsCurrentlyAudible()) {
      return;
    }
    expected_state_ = false;
    run_loop_.Run();
  }

 private:
  void OnAudioStateChanged(bool audible) override {
    EXPECT_EQ(expected_state_, audible);
    run_loop_.QuitWhenIdle();
  }

  base::RunLoop run_loop_;
  bool expected_state_ = false;
};

// Sets the extension to be enabled in incognito mode.
scoped_refptr<const Extension> SetExtensionIncognitoEnabled(
    const Extension& extension,
    Profile& profile) {
  // Enabling the extension in incognito results in an extension reload; wait
  // for that to finish and return the new extension pointer.
  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(&profile), extension.id());
  util::SetIsIncognitoEnabled(extension.id(), &profile, true);
  scoped_refptr<const Extension> reloaded_extension =
      registry_observer.WaitForExtensionLoaded();

  if (!reloaded_extension) {
    ADD_FAILURE() << "Failed to properly reload extension.";
    return nullptr;
  }

  EXPECT_TRUE(util::IsIncognitoEnabled(reloaded_extension->id(), &profile));
  return reloaded_extension;
}

// Wakes up the service worker for the `extension` in the given `profile`.
void WakeUpServiceWorker(const Extension& extension, Profile& profile) {
  base::RunLoop run_loop;
  const auto context_id = LazyContextId::ForExtension(&profile, &extension);
  ASSERT_TRUE(context_id.IsForServiceWorker());
  context_id.GetTaskQueue()->AddPendingTask(
      context_id,
      base::BindOnce([](std::unique_ptr<LazyContextTaskQueue::ContextInfo>) {
      }).Then(run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
}

}  // namespace

class OffscreenApiTest : public ExtensionApiTest {
 public:
  OffscreenApiTest() = default;
  ~OffscreenApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Add the kOffscreenDocumentTesting switch to allow the use of the
    // `TESTING` reason in offscreen document creation.
    command_line->AppendSwitch(switches::kOffscreenDocumentTesting);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // Creates a new offscreen document through an API call, expecting success.
  void ProgrammaticallyCreateOffscreenDocument(const Extension& extension,
                                               Profile& profile,
                                               const char* reason = "TESTING") {
    static constexpr char kScript[] =
        R"((async () => {
             let message;
             try {
               await chrome.offscreen.createDocument(
                   {
                     url: 'offscreen.html',
                     reasons: ['%s'],
                     justification: 'testing'
                   });
               message = 'success';
             } catch (e) {
               message = 'Error: ' + e.toString();
             }
             chrome.test.sendScriptResult(message);
           })();)";
    std::string script = base::StringPrintf(kScript, reason);
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        &profile, extension.id(), script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    ASSERT_TRUE(result.is_string());
    EXPECT_EQ("success", result.GetString());
  }

  // Closes an offscreen document through an API call, expecting success.
  void ProgrammaticallyCloseOffscreenDocument(const Extension& extension,
                                              Profile& profile) {
    static constexpr char kScript[] =
        R"((async () => {
             let message;
             try {
               await chrome.offscreen.closeDocument();
               message = 'success';
             } catch (e) {
               message = 'Error: ' + e.toString();
             }
             chrome.test.sendScriptResult(message);
           })();)";
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        &profile, extension.id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    ASSERT_TRUE(result.is_string());
    EXPECT_EQ("success", result.GetString());
  }

  // Returns the result of an API call to `runtime.getContexts()` to check if
  // an offscreen document exists. Expects the call to not throw an error,
  // independent of whether a document exists.
  bool ProgrammaticallyCheckIfHasOffscreenDocument(const Extension& extension,
                                                   Profile& profile) {
    static constexpr char kScript[] =
        R"((async () => {
             let result;
             try {
               const contexts =
                   await chrome.runtime.getContexts(
                       {contextTypes: ['OFFSCREEN_DOCUMENT']});
               if (!contexts || contexts.length > 1) {
                 throw new Error(
                     'Unexpected result: ' + JSON.stringify(contexts));
               }
               result = contexts.length == 1;
             } catch (e) {
               result = 'Error: ' + e.toString();
             }
             chrome.test.sendScriptResult(result);
           })();)";
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        &profile, extension.id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    EXPECT_TRUE(result.is_bool()) << result;
    return result.is_bool() && result.GetBool();
  }

 private:
  // chrome.runtime.getContexts(), used by these tests, is currently behind
  // a dev channel restriction.
  ScopedCurrentChannel current_channel_override_{version_info::Channel::DEV};
};

// Tests the general flow of creating an offscreen document.
#if BUILDFLAG(IS_MAC)
#define MAYBE_BasicDocumentManagement DISABLED_BasicDocumentManagement
#else
#define MAYBE_BasicDocumentManagement BasicDocumentManagement
#endif
IN_PROC_BROWSER_TEST_F(OffscreenApiTest, MAYBE_BasicDocumentManagement) {
  ASSERT_TRUE(RunExtensionTest("offscreen/basic_document_management"))
      << message_;
}

// Tests creating, querying, and closing offscreen documents in an incognito
// split mode extension.
// TODO(crbug.com/40282331): Disabled on ASAN due to leak caused by renderer gin
// objects which are intended to be leaked.
// TODO(crbug.com/345326424): Flaky on Mac builds.
#if defined(ADDRESS_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_IncognitoModeHandling_SplitMode \
  DISABLED_IncognitoModeHandling_SplitMode
#else
#define MAYBE_IncognitoModeHandling_SplitMode IncognitoModeHandling_SplitMode
#endif
IN_PROC_BROWSER_TEST_F(OffscreenApiTest,
                       MAYBE_IncognitoModeHandling_SplitMode) {
  // `split` incognito mode is required in order to allow the extension to
  // have a separate process in incognito.
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "permissions": ["offscreen"],
           "incognito": "split"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// Blank.");
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");

  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  extension = SetExtensionIncognitoEnabled(*extension, *profile());
  ASSERT_TRUE(extension);

  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_browser);
  Profile* incognito_profile = incognito_browser->profile();

  // We're going to be executing scripts in the service worker context, so
  // ensure the service worker is active.
  // TODO(devlin): Should BackgroundScriptExecutor handle that for us? (Perhaps
  // optionally?)
  WakeUpServiceWorker(*extension, *profile());
  WakeUpServiceWorker(*extension, *incognito_profile);

  auto has_offscreen_document = [this, extension](Profile& profile) {
    bool programmatic =
        ProgrammaticallyCheckIfHasOffscreenDocument(*extension, profile);
    bool in_manager =
        OffscreenDocumentManager::Get(&profile)
            ->GetOffscreenDocumentForExtension(*extension) != nullptr;
    EXPECT_EQ(programmatic, in_manager) << "Mismatch between manager and API.";
    return programmatic && in_manager;
  };

  // Create an offscreen document in the on-the-record profile. Only it should
  // have a document; the off-the-record profile is considered distinct.
  ProgrammaticallyCreateOffscreenDocument(*extension, *profile());
  EXPECT_TRUE(has_offscreen_document(*profile()));
  EXPECT_FALSE(has_offscreen_document(*incognito_profile));

  // Now, create a new document in the off-the-record profile.
  ProgrammaticallyCreateOffscreenDocument(*extension,
                                          *incognito_browser->profile());
  EXPECT_TRUE(has_offscreen_document(*profile()));
  EXPECT_TRUE(has_offscreen_document(*incognito_profile));

  // Close the off-the-record profile - the on-the-record profile's offscreen
  // document should remain open.
  ProgrammaticallyCloseOffscreenDocument(*extension, *incognito_profile);
  EXPECT_TRUE(has_offscreen_document(*profile()));
  EXPECT_FALSE(has_offscreen_document(*incognito_profile));

  // Finally, close the on-the-record profile's document.
  ProgrammaticallyCloseOffscreenDocument(*extension, *profile());
  EXPECT_FALSE(has_offscreen_document(*profile()));
  EXPECT_FALSE(has_offscreen_document(*incognito_profile));
}

// Tests creating, querying, and closing offscreen documents in an incognito
// spanning mode extension.
// TODO(crbug.com/40282331): Disabled on ASAN due to leak caused by renderer gin
// objects which are intended to be leaked.
// TODO(crbug.com/345326424): Flaky on Mac builds.
#if defined(ADDRESS_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_IncognitoModeHandling_SpanningMode \
  DISABLED_IncognitoModeHandling_SpanningMode
#else
#define MAYBE_IncognitoModeHandling_SpanningMode \
  IncognitoModeHandling_SpanningMode
#endif
IN_PROC_BROWSER_TEST_F(OffscreenApiTest,
                       MAYBE_IncognitoModeHandling_SpanningMode) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "permissions": ["offscreen"],
           "incognito": "spanning"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// Blank.");
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");

  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  extension = SetExtensionIncognitoEnabled(*extension, *profile());
  ASSERT_TRUE(extension);

  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_browser);
  Profile* incognito_profile = incognito_browser->profile();

  // Wake up the on-the-record service worker (the only one we have, as a
  // spanning mode extension).
  WakeUpServiceWorker(*extension, *profile());

  auto has_offscreen_document = [this, extension](Profile& profile) {
    bool programmatic =
        ProgrammaticallyCheckIfHasOffscreenDocument(*extension, profile);
    bool in_manager =
        OffscreenDocumentManager::Get(&profile)
            ->GetOffscreenDocumentForExtension(*extension) != nullptr;
    EXPECT_EQ(programmatic, in_manager) << "Mismatch between manager and API.";
    return programmatic && in_manager;
  };

  // There's less to do in a spanning mode extension - by definition, we can't
  // call any methods from an incognito profile, so we just have to verify that
  // the incognito profile is unaffected.
  ProgrammaticallyCreateOffscreenDocument(*extension, *profile());
  EXPECT_TRUE(has_offscreen_document(*profile()));
  // Don't use `has_offscreen_document()` since we can't actually check the
  // programmatic status, which requires executing script in an incognito
  // process.
  OffscreenDocumentManager* incognito_manager =
      OffscreenDocumentManager::Get(incognito_profile);
  EXPECT_EQ(nullptr,
            incognito_manager->GetOffscreenDocumentForExtension(*extension));

  ProgrammaticallyCloseOffscreenDocument(*extension, *profile());
  EXPECT_FALSE(has_offscreen_document(*profile()));
  EXPECT_EQ(nullptr,
            incognito_manager->GetOffscreenDocumentForExtension(*extension));
}

IN_PROC_BROWSER_TEST_F(OffscreenApiTest, LifetimeEnforcement) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "permissions": ["offscreen"]
         })";
  // An offscreen document that knows how to play audio.
  static constexpr char kOffscreenJs[] =
      R"(function playAudio() {
           const audioTag = document.createElement('audio');
           audioTag.src = '_test_resources/long_audio.ogg';
           document.body.appendChild(audioTag);
           audioTag.play();
         }

         function stopAudio() {
           document.body.getElementsByTagName('audio')[0].pause();
         }

         chrome.runtime.onMessage.addListener((msg) => {
           if (msg == 'play')
             playAudio();
           else if (msg == 'stop')
             stopAudio();
           else
             console.error('Unexpected message: ' + msg);
         }))";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// Blank.");
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     R"(<html><script src="offscreen.js"></script></html>)");
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.js"), kOffscreenJs);

  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Create a new offscreen document for audio playback and wait for it to load.
  OffscreenDocumentManager* manager = OffscreenDocumentManager::Get(profile());
  ProgrammaticallyCreateOffscreenDocument(*extension, *profile(),
                                          "AUDIO_PLAYBACK");
  OffscreenDocumentHost* document =
      manager->GetOffscreenDocumentForExtension(*extension);
  ASSERT_TRUE(document);
  content::WaitForLoadStop(document->host_contents());

  // Begin the audio playback.
  {
    AudioWaiter audio_waiter(document->host_contents());
    BackgroundScriptExecutor::ExecuteScriptAsync(
        profile(), extension->id(), "chrome.runtime.sendMessage('play');");
    audio_waiter.WaitForAudible();
  }

  // The document should be kept alive.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(manager->GetOffscreenDocumentForExtension(*extension));

  // Override the timeout. We can't do this at the top of the test because
  // otherwise, the document would immediately be considered inactive.
  auto timeout_override =
      AudioLifetimeEnforcer::SetTimeoutForTesting(base::Seconds(0));

  // Now, stop the audio.
  {
    AudioWaiter audio_waiter(document->host_contents());
    BackgroundScriptExecutor::ExecuteScriptAsync(
        profile(), extension->id(), "chrome.runtime.sendMessage('stop');");
    audio_waiter.WaitForInaudible();
  }

  // The offscreen document should be closed since it is no longer active.
  // `document` is now unsafe to use.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager->GetOffscreenDocumentForExtension(*extension));
}

// Tests opening and immediately closing an offscreen document (so that the
// close happens before it's fully loaded). Regression test for
// https://crbug.com/1450784.
IN_PROC_BROWSER_TEST_F(OffscreenApiTest, OpenAndImmediatelyCloseDocument) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "permissions": ["offscreen"]
         })";
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function openAndRapidlyClose() {
             const openResult =
                 chrome.offscreen.createDocument(
                     {
                       url: 'offscreen.html',
                       reasons: ['TESTING'],
                       justification: 'Testing'
                     });
             chrome.offscreen.closeDocument();
             await chrome.test.assertPromiseRejects(
                 openResult,
                 'Error: Offscreen document closed before fully loading.');
             chrome.test.succeed();
           },
         ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"), "<html></html>");

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

// TODO(crbug.com/40272130): Failing on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TabCaptureStreams DISABLED_TabCaptureStreams
#else
#define MAYBE_TabCaptureStreams TabCaptureStreams
#endif
IN_PROC_BROWSER_TEST_F(OffscreenApiTest, MAYBE_TabCaptureStreams) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("offscreen/tab_capture_streams"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));

  // Tab capture requires active tab, so click on the action to grant permission
  // and kick off the tests.
  ResultCatcher result_catcher;
  ExtensionActionTestHelper::Create(browser())->Press(extension->id());
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

class OffscreenApiTestWithoutCommandLineFlag : public OffscreenApiTest {
 public:
  OffscreenApiTestWithoutCommandLineFlag() = default;
  ~OffscreenApiTestWithoutCommandLineFlag() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Explicitly don't call OffscreenApiTest's version to avoid adding the
    // commandline flag.
    ExtensionApiTest::SetUpCommandLine(command_line);
  }
};

// Tests opening an offscreen document that takes awhile to load properly waits
// for the document to load before resolving the promise, ensuring the document
// is ready to receive messages by the time the promise resolves.
IN_PROC_BROWSER_TEST_F(OffscreenApiTest, LongLoadOffscreenDocument) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["offscreen"],
           "background": { "service_worker": "background.js" }
         })";
  static constexpr char kOffscreenHtml[] =
      R"(<html><script src="offscreen.js"></script></html>)";
  // This script busy-waits for two seconds before (synchronously) adding a
  // message listener.
  static constexpr char kOffscreenJs[] =
      R"(const startTime = performance.now();
         const endTime = startTime + 2000;
         while (performance.now() < endTime) { /* Spin our wheels! */ }
         chrome.runtime.onMessage.addListener((msg, sender, reply) => {
           reply(msg + ' reply');
         });)";
  // The background script will open an offscreen document and, once the
  // createDocument() call resolves, send a message. Since createDocument()
  // should wait for document to finish loading, this should work.
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function longLoadDocAndSendMessage() {
             await chrome.offscreen.createDocument(
                       {
                           url: 'offscreen.html',
                           reasons: ['TESTING'],
                           justification: 'testing'
                       });
             const reply = await chrome.runtime.sendMessage('test message');
             chrome.test.assertEq('test message reply', reply);
             chrome.test.succeed();
           },
         ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"), kOffscreenHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.js"), kOffscreenJs);

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

// Tests user gestures are curried from service workers into offscreen
// documents.
IN_PROC_BROWSER_TEST_F(OffscreenApiTest,
                       UserGesturesAreCurriedFromServiceWorkers) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["offscreen"],
           "action": {},
           "background": { "service_worker": "background.js" }
         })";
  static constexpr char kOffscreenHtml[] =
      R"(<html><script src="offscreen.js"></script></html>)";
  static constexpr char kOffscreenJs[] =
      R"(chrome.runtime.onMessage.addListener((msg, sender, sendReply) => {
           try {
             const activeGesture = chrome.test.isProcessingUserGesture();
             sendReply('active gesture: ' + activeGesture);
           } catch (e) {
             sendReply(`Error: ${e.toString()}`);
           }
         });)";
  // The extension background script will:
  // - Open a new offscreen document
  // - Wait for an action click. This includes an active user action.
  // - In the listener for the action click, dispatch a message to the
  //   offscreen document. The active user gesture should be curried along.
  static constexpr char kBackgroundJs[] =
      R"((async () => {
             await chrome.offscreen.createDocument(
                       {
                           url: 'offscreen.html',
                           reasons: ['TESTING'],
                           justification: 'testing'
                       });
             chrome.test.sendMessage('opened');
         })();
         chrome.action.onClicked.addListener(() => {
           chrome.test.assertTrue(chrome.test.isProcessingUserGesture());
           chrome.runtime.sendMessage('test message').then(response => {
             chrome.test.assertEq('active gesture: true', response);
             chrome.test.succeed();
           });
         });)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"), kOffscreenHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.js"), kOffscreenJs);

  ExtensionTestMessageListener test_listener("opened");
  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(test_listener.WaitUntilSatisfied());
  ExtensionActionTestHelper::Create(browser())->Press(extension->id());
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Tests that the `TESTING` reason is disallowed without the appropriate
// commandline switch.
IN_PROC_BROWSER_TEST_F(OffscreenApiTestWithoutCommandLineFlag,
                       TestingReasonNotAllowed) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["offscreen"],
           "background": { "service_worker": "background.js" }
         })";
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function cannotCreateDocumentWithTestingReason() {
             await chrome.test.assertPromiseRejects(
                 chrome.offscreen.createDocument(
                     {
                         url: 'offscreen.html',
                         reasons: ['TESTING'],
                         justification: 'testing'
                     }),
                 'Error: The `TESTING` reason is only available with the ' +
                 '--offscreen-document-testing commandline switch applied.');
             chrome.test.succeed();
           },
         ]);)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"), "<html></html>");

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

}  // namespace extensions
