// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_api.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/channel.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/offscreen/offscreen_document_manager.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

class ContentBrowserClientMock : public ChromeContentBrowserClient {
 public:
  MOCK_METHOD(bool,
              IsGetAllScreensMediaAllowed,
              (content::BrowserContext * context, const url::Origin& origin),
              (override));
};

}  // namespace

class GetAllScreensMediaOffscreenApiTest : public ExtensionApiTest {
 public:
  GetAllScreensMediaOffscreenApiTest() = default;
  ~GetAllScreensMediaOffscreenApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    browser_client_ = std::make_unique<ContentBrowserClientMock>();
    content::SetBrowserClientForTesting(browser_client_.get());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitFromCommandLine(
        /*enable_features=*/
        "GetAllScreensMedia",
        /*disable_features=*/"");
  }

  // Creates a new offscreen document through an API call, expecting success.
  void ProgrammaticallyCreateOffscreenDocument(const Extension& extension,
                                               Profile& profile,
                                               const char* reason) {
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

 protected:
  ContentBrowserClientMock& content_browser_client() {
    return *browser_client_;
  }

 private:
  // chrome.runtime.getContexts(), used by these tests, is currently behind
  // a dev channel restriction.
  ScopedCurrentChannel current_channel_override_{version_info::Channel::DEV};
  std::unique_ptr<ContentBrowserClientMock> browser_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test checks if the `getAllScreensMedia` API is available and fully
// functional in offscreen documents on ChromeOS lacros.
IN_PROC_BROWSER_TEST_F(GetAllScreensMediaOffscreenApiTest,
                       GetAllScreensMediaAllowed) {
  // This test corresponds to a critical user journey (CUJ)
  // (go/cros-cuj-tracker) for ChromeOS commercial.
  // This tag links the test to a CUJ and allows close tracking whether a user
  // journey is fully functional.
  base::AddTagToTestResult("feature_id",
                           "screenplay-f3601ae4-bff7-495a-a51f-3c0997a46445");
  EXPECT_CALL(content_browser_client(),
              IsGetAllScreensMediaAllowed(testing::_, testing::_))
      .WillOnce(testing::Return(true));

  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "permissions": ["offscreen"]
         })";
  // An offscreen document that knows how to capture all screens.
  static constexpr char kOffscreenJs[] =
      R"(
        let streams;

        async function captureAllScreens() {
          try {
            streams = await navigator.mediaDevices.getAllScreensMedia();
            if (streams === null || streams.length == 0) {
              return false;
            }

            let allStreamsOk = true;
            streams.forEach((stream) => {
              const videoTracks = stream.getVideoTracks();
              if (videoTracks.length == 0) {
                allStreamsOk = false;
                return;
              }

              const videoTrack = videoTracks[0];
              if (typeof videoTrack.screenDetailed !== "function") {
                allStreamsOk = false;
                return;
              }
            });

            chrome.test.sendScriptResult(allStreamsOk);
            return;
          } catch(e) {
            console.error('Unexcpected exception: ' + e);
            chrome.test.sendScriptResult(false);
          }
        }

        function stopCapture() {
          streams.forEach((stream) => {
            stream.getVideoTracks()[0].stop();
          });
        }

        chrome.runtime.onMessage.addListener(async (msg) => {
          if (msg == 'capture') {
            await captureAllScreens();
          } else if (msg == 'stop') {
            stopCapture();
          } else {
            console.error('Unexpected message: ' + msg);
          }
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
                                          "DISPLAY_MEDIA");
  OffscreenDocumentHost* document =
      manager->GetOffscreenDocumentForExtension(*extension);
  ASSERT_TRUE(document);
  content::WaitForLoadStop(document->host_contents());

  // Begin the screen capture.
  {
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), "chrome.runtime.sendMessage('capture');",
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    ASSERT_TRUE(result.is_bool());
    EXPECT_TRUE(result.GetBool());
  }

  // The document should be kept alive.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(manager->GetOffscreenDocumentForExtension(*extension));

  // Now, stop the capture.
  {
    BackgroundScriptExecutor::ExecuteScriptAsync(
        profile(), extension->id(), "chrome.runtime.sendMessage('stop');");
  }

  // TODO(crbug.com/1443432): Add check if document gets shut down after the
  // screen capture with `getAllScreensMedia` is stopped.
}

}  // namespace extensions
