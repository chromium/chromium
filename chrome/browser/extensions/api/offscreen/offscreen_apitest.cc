// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_api.h"

#include <algorithm>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/offscreen/offscreen_document_manager.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

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
  auto quit_loop_adapter =
      [&run_loop](std::unique_ptr<LazyContextTaskQueue::ContextInfo>) {
        run_loop.QuitWhenIdle();
      };
  ServiceWorkerTaskQueue::Get(&profile)->AddPendingTask(
      LazyContextId(&profile, extension.id(), extension.url()),
      base::BindLambdaForTesting(quit_loop_adapter));
  run_loop.Run();
}

}  // namespace

class OffscreenApiTest : public ExtensionApiTest {
 public:
  OffscreenApiTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsOffscreenDocuments);
  }
  ~OffscreenApiTest() override = default;

  // Creates a new offscreen document through an API call, expecting success.
  void ProgrammaticallyCreateOffscreenDocument(const Extension& extension,
                                               Profile& profile) {
    static constexpr char kScript[] =
        R"((async () => {
             let message;
             try {
               await chrome.offscreen.createDocument(
                   {
                     url: 'offscreen.html',
                     reasons: ['TESTING'],
                     justification: 'testing'
                   });
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

 private:
  // The `offscreen` API is currently behind both a feature and a channel
  // restriction.
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_channel_override_{version_info::Channel::CANARY};
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

class OffscreenApiTestWithoutFeature : public ExtensionApiTest {
 public:
  OffscreenApiTestWithoutFeature() = default;
  ~OffscreenApiTestWithoutFeature() override = default;

 private:
  ScopedCurrentChannel current_channel_override_{
      version_info::Channel::UNKNOWN};
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
  std::transform(install_warnings.begin(), install_warnings.end(),
                 std::back_inserter(string_warnings),
                 [](const InstallWarning& warning) { return warning.message; });

  static constexpr char kExpectedWarning[] =
      "'offscreen' requires the 'ExtensionsOffscreenDocuments' feature flag to "
      "be enabled.";
  EXPECT_THAT(string_warnings, testing::ElementsAre(kExpectedWarning));
}

}  // namespace extensions
