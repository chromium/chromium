// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/download/public/common/download_item.h"
#include "components/permissions/mock_chooser_controller_view.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "extensions/common/extension_features.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

using testing::Contains;
using testing::StartsWith;

namespace controlled_frame {

class ControlledFramePermissionRequestTest
    : public ControlledFramePermissionRequestTestBase,
      public testing::WithParamInterface<PermissionRequestTestParam> {};

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestTest, Camera) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      const constraints = { video: true };
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);

        if(stream.getVideoTracks().length > 0){
          return 'SUCCESS';
        }
        return 'FAIL: ' + stream.getVideoTracks().length + ' tracks';
      } catch (err) {
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )";
  test_case.permission_name = "media";
  test_case.policy_features.insert(
      {blink::mojom::PermissionsPolicyFeature::kCamera});
  test_case.content_settings_type.insert(
      {ContentSettingsType::MEDIASTREAM_CAMERA});

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestTest, Microphone) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      const constraints = { audio: true };
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);

        if(stream.getAudioTracks().length > 0){
          return 'SUCCESS';
        }
        return 'FAIL: ' + stream.getAudioTracks().length + ' tracks';
      } catch (err) {
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )";
  test_case.permission_name = "media";
  test_case.policy_features.insert(
      {blink::mojom::PermissionsPolicyFeature::kMicrophone});
  test_case.content_settings_type.insert(
      {ContentSettingsType::MEDIASTREAM_MIC});

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestTest, Geolocation) {
  device::ScopedGeolocationOverrider overrider(/*latitude=*/1, /*longitude=*/2);

  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      try {
        return await new Promise((resolve, reject) => {
          navigator.geolocation.getCurrentPosition(
            (position) => {
              resolve('SUCCESS');
            },
            (error) => {
              const errorMessage = 'FAIL: ' + error.code + error.message;
              resolve(errorMessage);
            }
          );
        });
      } catch (err) {
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )";
  test_case.permission_name = "geolocation";
  test_case.policy_features.insert(
      {blink::mojom::PermissionsPolicyFeature::kGeolocation});
  test_case.content_settings_type.insert({ContentSettingsType::GEOLOCATION});

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestTest,
                       RequestFileSystem) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      return new Promise((resolve) => {
        window.requestFileSystem = window.requestFileSystem ||
                                   window.webkitRequestFileSystem;

        if (!window.requestFileSystem) {
          resolve("FAILURE: This browser does not support requestFileSystem.");
          return;
        }

        const storageType = window.PERSISTENT;
        const requestedBytes = 1024 * 1024;

        window.requestFileSystem(storageType, requestedBytes,
          (fileSystem) => {
            resolve("SUCCESS");
          },
          (error) => {
            resolve("FAILURE: " + error.message);
          }
        );
      });
    })();
  )";
  test_case.permission_name = "filesystem";

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

class TestDownloadManagerObserver : public content::DownloadManager::Observer {
 public:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    CHECK(item);
    downloads_.push_back(item->GetSuggestedFilename());
  }

  const std::vector<std::string>& Downloads() const { return downloads_; }

 private:
  std::vector<std::string> downloads_;
};

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestTest, Download) {
  const std::string download_script = R"(
    (function() {
      try {
          const link = document.createElement("a");
          link.download = $1;
          link.href = $1;
          link.click();
          return 'SUCCESS';
      } catch (err) {
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )";

  PermissionRequestTestCase test_case;
  test_case.test_script =
      content::JsReplace(download_script, "download_test.zip");
  test_case.permission_name = "download";

  PermissionRequestTestParam test_param = GetParam();

  content::DownloadTestObserverTerminal completion_observer(
      profile()->GetDownloadManager(), test_param.expected_success ? 2 : 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  TestDownloadManagerObserver download_observer;
  profile()->GetDownloadManager()->AddObserver(&download_observer);

  VerifyEnabledPermission(
      test_case, test_param,
      base::BindLambdaForTesting(
          [](bool should_success) -> std::string { return "SUCCESS"; }));

  // If |completion_observer| is expecting 0 downloads, then it will not wait
  // for unexpected downloads. To avoid this, We execute another download in a
  // normal tab, so at least one download will be waited on.
  {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    ASSERT_TRUE(content::NavigateToURL(
        web_contents, embedded_https_test_server().base_url()));

    ASSERT_THAT(content::EvalJs(web_contents->GetPrimaryMainFrame(),
                                content::JsReplace(download_script,
                                                   "download_baseline.txt"))
                    .ExtractString(),
                StartsWith("SUCCESS"));
  }

  completion_observer.WaitForFinished();

  EXPECT_EQ(download_observer.Downloads().size(),
            test_param.expected_success ? 2ul : 1ul);

  EXPECT_THAT(download_observer.Downloads(), Contains("download_baseline.txt"));
  if (download_observer.Downloads().size() == 2ul) {
    EXPECT_THAT(download_observer.Downloads(), Contains("download_test.zip"));
  }

  profile()->GetDownloadManager()->RemoveObserver(&download_observer);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/
                         ,
                         ControlledFramePermissionRequestTest,
                         testing::ValuesIn(
                             GetDefaultPermissionRequestTestParams()),
                         [](const testing::TestParamInfo<
                             PermissionRequestTestParam>& info) {
                           return info.param.name;
                         });

class MockHidDelegate : public ChromeHidDelegate {
 public:
  // Simulates opening the HID device chooser dialog and selecting an item. The
  // chooser automatically selects the device under index 0.
  void OnWebViewHidPermissionRequestCompleted(
      base::WeakPtr<HidChooser> chooser,
      content::GlobalRenderFrameHostId embedder_rfh_id,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
      content::HidChooser::Callback callback,
      bool allow) override {
    if (!allow) {
      std::move(callback).Run(std::vector<device::mojom::HidDeviceInfoPtr>());
      return;
    }

    auto* render_frame_host = content::RenderFrameHost::FromID(embedder_rfh_id);
    ASSERT_TRUE(render_frame_host);

    chooser_controller_ = std::make_unique<HidChooserController>(
        render_frame_host, std::move(filters), std::move(exclusion_filters),
        std::move(callback));

    mock_chooser_view_ =
        std::make_unique<permissions::MockChooserControllerView>();
    chooser_controller_->set_view(mock_chooser_view_.get());

    EXPECT_CALL(*mock_chooser_view_.get(), OnOptionsInitialized)
        .WillOnce(
            testing::Invoke([this] { chooser_controller_->Select({0}); }));
  }

 private:
  std::unique_ptr<HidChooserController> chooser_controller_;
  std::unique_ptr<permissions::MockChooserControllerView> mock_chooser_view_;
};

class TestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  // ContentBrowserClient:
  content::HidDelegate* GetHidDelegate() override { return &delegate_; }

 private:
  MockHidDelegate delegate_;
};

class ControlledFramePermissionRequestWebHidTest
    : public ControlledFramePermissionRequestTest {
 public:
  void SetUpOnMainThread() override {
    ControlledFramePermissionRequestTest::SetUpOnMainThread();

    original_client_ = content::SetBrowserClientForTesting(&overriden_client_);

    mojo::PendingRemote<device::mojom::HidManager> pending_remote;
    hid_manager_.Bind(pending_remote.InitWithNewPipeAndPassReceiver());
    base::test::TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>>
        devices_future;
    auto* chooser_context = HidChooserContextFactory::GetForProfile(profile());
    chooser_context->SetHidManagerForTesting(std::move(pending_remote),
                                             devices_future.GetCallback());
    ASSERT_TRUE(devices_future.Wait());

    hid_manager_.CreateAndAddDevice("1", 0, 0, "Test HID Device", "",
                                    device::mojom::HidBusType::kHIDBusTypeUSB);
  }

  ~ControlledFramePermissionRequestWebHidTest() override {
    content::SetBrowserClientForTesting(original_client_.get());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      extensions_features::kEnableWebHidInWebView};
  TestContentBrowserClient overriden_client_;
  raw_ptr<content::ContentBrowserClient> original_client_ = nullptr;
  device::FakeHidManager hid_manager_;
};

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestWebHidTest, WebHid) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function () {
      try {
        const device_filters = [{vendorId: 0}];
        const device = await navigator.hid.requestDevice({
          filters: device_filters});
        if (device.length > 0){
          return 'SUCCESS';
        }
        return 'FAIL: device length ' + device.length;
      } catch (error) {
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )";
  test_case.permission_name = "hid";

  test_case.policy_features.insert(
      {blink::mojom::PermissionsPolicyFeature::kHid});
  // No embedder content settings for WebHid.

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/
                         ,
                         ControlledFramePermissionRequestWebHidTest,
                         testing::ValuesIn(
                             GetDefaultPermissionRequestTestParams()),
                         [](const testing::TestParamInfo<
                             PermissionRequestTestParam>& info) {
                           return info.param.name;
                         });

}  // namespace controlled_frame
