// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_api.h"

#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/offscreen/audio_lifetime_enforcer.h"
#include "extensions/browser/api/offscreen/offscreen_document_manager.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

// A helper class to wait until a given WebContents is audible or inaudible.
// TODO(devlin): Put this somewhere common? //content/public/test/?
class AudioWaiter : public content::WebContentsObserver {
 public:
  explicit AudioWaiter(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}

  void WaitForAudible() {
    if (web_contents()->IsCurrentlyAudible())
      return;
    expected_state_ = true;
    run_loop_.Run();
  }

  void WaitForInaudible() {
    if (!web_contents()->IsCurrentlyAudible())
      return;
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
  ServiceWorkerTaskQueue::Get(&profile)->AddPendingTask(
      LazyContextId(&profile, extension.id(), extension.url()),
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

  // Returns the result of an API call to `offscreen.hasDocument()`. Expects the
  // call to not throw an error, independent of whether a document exists.
  bool ProgrammaticallyCheckIfHasOffscreenDocument(const Extension& extension,
                                                   Profile& profile) {
    static constexpr char kScript[] =
        R"((async () => {
             let result;
             try {
               result = await chrome.offscreen.hasDocument();
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
};

// Tests the general flow of creating an offscreen document.
IN_PROC_BROWSER_TEST_F(OffscreenApiTest, BasicDocumentManagement) {
  ASSERT_TRUE(RunExtensionTest("offscreen/basic_document_management"))
      << message_;
}

// Tests creating, querying, and closing offscreen documents in an incognito
// split mode extension.
IN_PROC_BROWSER_TEST_F(OffscreenApiTest, IncognitoModeHandling_SplitMode) {
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
IN_PROC_BROWSER_TEST_F(OffscreenApiTest, IncognitoModeHandling_SpanningMode) {
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

class OffscreenApiTestWithoutFeature : public ExtensionApiTest {
 public:
  OffscreenApiTestWithoutFeature() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionsOffscreenDocuments);
  }
  ~OffscreenApiTestWithoutFeature() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the `offscreen` API is unavailable if the requisite feature
// (`ExtensionsOffscreenDocuments`) is not enabled. We have this explicit test
// mostly to double-check our registration, since features are prone to typos.
IN_PROC_BROWSER_TEST_F(OffscreenApiTestWithoutFeature,
                       APIUnavailableWithoutFeature) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["offscreen"],
           "background": { "service_worker": "background.js" }
         })";
  // The extension validates the `offscreen` API is undefined.
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function apiIsUnavailable() {
             chrome.test.assertEq(undefined, chrome.offscreen);
             chrome.test.succeed();
           },
         ]);)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.ignore_manifest_warnings = true});
  ASSERT_TRUE(extension);

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  // An install warning should be emitted since the extension requested a
  // restricted permission.
  const std::vector<InstallWarning>& install_warnings =
      extension->install_warnings();

  // Turn our InstallWarnings into strings for easier testing.
  std::vector<std::string> string_warnings;
  base::ranges::transform(install_warnings, std::back_inserter(string_warnings),
                          &InstallWarning::message);

  static constexpr char kExpectedWarning[] =
      "'offscreen' requires the 'ExtensionsOffscreenDocuments' feature flag to "
      "be enabled.";
  EXPECT_THAT(string_warnings, testing::ElementsAre(kExpectedWarning));
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
