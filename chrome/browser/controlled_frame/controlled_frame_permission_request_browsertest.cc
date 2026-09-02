// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/download/public/common/download_item.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/permissions/mock_chooser_controller_view.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/common/extension_features.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

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
      {network::mojom::PermissionsPolicyFeature::kCamera});
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
      {network::mojom::PermissionsPolicyFeature::kMicrophone});
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
      {network::mojom::PermissionsPolicyFeature::kGeolocation});
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

// TODO(crbug.com/422421852): These tests require document focus,
// and waiting for focus is flaky on mac.
#if BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_ClipboardReadWrite DISABLED_ClipboardReadWrite
#define MAYBE_ClipboardSanitizedWrite DISABLED_ClipboardSanitizedWrite
#else
#define MAYBE_ClipboardReadWrite ClipboardReadWrite
#define MAYBE_ClipboardSanitizedWrite ClipboardSanitizedWrite
#endif
IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestTest,
                       MAYBE_ClipboardReadWrite) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      try {
        return await new Promise((resolve, reject) => {
          if (!document.hasFocus()) {
            resolve('Document must have focus');
            return;
          }
          if (!navigator.userActivation.isActive) {
            resolve('User activation must be true');
            return;
          }
          navigator.clipboard.readText().then(
            (text) => {
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
  test_case.permission_name = "clipboardReadWrite";
  test_case.policy_features.insert(
      {network::mojom::PermissionsPolicyFeature::kClipboardRead});
  test_case.content_settings_type.insert(
      {ContentSettingsType::CLIPBOARD_READ_WRITE});
  test_case.must_wait_for_document_focus = true;

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestTest,
                       MAYBE_ClipboardSanitizedWrite) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      try {
        return await new Promise((resolve, reject) => {
          if (!document.hasFocus()) {
            resolve('Document must have focus');
            return;
          }
          if (!navigator.userActivation.isActive) {
            resolve('User activation must be true');
            return;
          }
          navigator.clipboard.writeText('test text').then(
            () => {
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

  // Don't add ContentSettingType because
  // the embedder has hardcoded ContentSetting::CONTENT_SETTING_ALLOW.
  test_case.permission_name = "clipboardSanitizedWrite";
  test_case.policy_features.insert(
      {network::mojom::PermissionsPolicyFeature::kClipboardWrite});
  test_case.must_wait_for_document_focus = true;

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
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
        .WillOnce([this] { chooser_controller_->Select({0}); });
  }

 private:
  std::unique_ptr<permissions::MockChooserControllerView> mock_chooser_view_;
  std::unique_ptr<HidChooserController> chooser_controller_;
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
      {network::mojom::PermissionsPolicyFeature::kHid});
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

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestWebHidTest,
                       PolicyGrantDoesNotBypassEmbedderForGuest) {
  // 1. Install and open IWA, then create ControlledFrame.
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt, "/empty.html");
  ASSERT_TRUE(app_frame);
  ASSERT_TRUE(controlled_frame);

  url::Origin guest_origin = controlled_frame->GetLastCommittedOrigin();

  // 2. Set up policy allowing the guest origin.
  // Note: vendor_id 0 and product_id 0 match the default device added in
  // ControlledFramePermissionRequestWebHidTest::SetUpOnMainThread.
  const char kPolicySetting[] = R"(
      [
        {
          "devices": [{ "vendor_id": 0, "product_id": 0 }],
          "urls": ["%s"]
        }
      ])";
  g_browser_process->local_state()->Set(
      prefs::kManagedWebHidAllowDevicesForUrls,
      base::test::ParseJson(base::StringPrintf(
          kPolicySetting, guest_origin.Serialize().c_str())));

  // 3. Guest calls getDevices() and should NOT see the device because it needs
  // embedder delegation.
  constexpr char kTestScript[] = R"(
    (async function() {
      try {
        const devices = await navigator.hid.getDevices();
        if (devices.length === 0) {
          return 'SUCCESS: NO_DEVICES';
        }
        return 'FAIL: Got ' + devices.length + ' devices';
      } catch (err) {
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )";

  EXPECT_EQ("SUCCESS: NO_DEVICES",
            content::EvalJs(controlled_frame, kTestScript).ExtractString());
}

class ControlledFramePermissionStatusLeakTest : public ControlledFrameTestBase {
 protected:
  void SetPermission(const GURL& url,
                     ContentSettingsType type,
                     ContentSetting setting) {
    HostContentSettingsMapFactory::GetForProfile(profile())
        ->SetContentSettingDefaultScope(url, url, type, setting);
  }

  std::string QueryPermission(content::RenderFrameHost* frame,
                              const std::string& name) {
    return content::EvalJs(frame, content::JsReplace(R"(
      navigator.permissions.query({name: $1}).then(r => r.state);
    )",
                                                     name))
        .ExtractString();
  }
};

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionStatusLeakTest,
                       PermissionsStatusDoNotLeak) {
  GURL guest_url =
      embedded_https_test_server().GetURL("guest.com", "/empty.html");
  url::Origin guest_origin = url::Origin::Create(guest_url);

  // Set profile-wide permissions for the guest origin.
  SetPermission(guest_url, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_ALLOW);
  SetPermission(guest_url, ContentSettingsType::MEDIASTREAM_MIC,
                CONTENT_SETTING_BLOCK);
  SetPermission(guest_url, ContentSettingsType::GEOLOCATION,
                CONTENT_SETTING_ALLOW);
  SetPermission(guest_url, ContentSettingsType::CLIPBOARD_READ_WRITE,
                CONTENT_SETTING_ALLOW);

  // Install and open IWA, then create ControlledFrame pointing to the guest
  // origin.
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/"guest.com", "/empty.html");
  ASSERT_TRUE(app_frame);
  ASSERT_TRUE(controlled_frame);
  ASSERT_EQ(controlled_frame->GetLastCommittedOrigin(), guest_origin);

  // Geolocation (control case, already overridden to ASK, should return
  // "prompt")
  EXPECT_EQ("prompt", QueryPermission(controlled_frame, "geolocation"));

  // Camera, Microphone and Clipboard should also be isolated and return
  // "prompt". Before the fix, these will leak.
  EXPECT_EQ("prompt", QueryPermission(controlled_frame, "camera"));
  EXPECT_EQ("prompt", QueryPermission(controlled_frame, "microphone"));
  EXPECT_EQ("prompt", QueryPermission(controlled_frame, "clipboard-read"));
}

class ControlledFramePermissionRequestPEPCTest
    : public ControlledFramePermissionRequestTest {
 public:
  ControlledFramePermissionRequestPEPCTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kUserMediaElement,
         blink::features::kUserMediaElementLegacy,
         blink::features::kBypassPepcSecurityForTesting},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestPEPCTest, Camera) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      const pepc = document.createElement('usermedia');
      pepc.type = 'camera';
      pepc.setConstraints({video: {}});
      document.body.appendChild(pepc);

      const status = await navigator.permissions.query({name: 'camera'});

      if (status.state !== 'prompt') {
        // If the permission state is already 'granted' or 'denied', the PEPC
        // <usermedia> element is not active (either in no-op or disabled state)
        // and clicking it will not trigger a prompt. We bypass the click and
        // verify the aggregate state using standard getUserMedia instead.
        try {
          const stream =
              await navigator.mediaDevices.getUserMedia({video: true});
          return (stream.getVideoTracks().length > 0) ? 'SUCCESS' : 'FAIL';
        } catch (err) {
          return 'FAIL';
        }
      }

      // Wait for two animation frames to ensure the PEPC element is fully laid
      // out and rendered by the browser before we simulate the click.
      // Programmatic clicks on unrendered elements are ignored by Blink's
      // security engine.
      await new Promise((resolve) => {
        window.requestAnimationFrame(() => {
          window.requestAnimationFrame(resolve);
        });
      });

      const promise = new Promise((resolve) => {
        pepc.addEventListener('promptaction', () => {
          resolve(pepc.matches(':granted') ? 'SUCCESS' : 'FAIL');
        });
      });

      pepc.click();
      return await promise;
    })();
  )";
  test_case.permission_name = "media";
  test_case.policy_features.insert(
      {network::mojom::PermissionsPolicyFeature::kCamera});
  test_case.content_settings_type.insert(
      {ContentSettingsType::MEDIASTREAM_CAMERA});

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestPEPCTest, Microphone) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      const pepc = document.createElement('usermedia');
      pepc.type = 'microphone';
      pepc.setConstraints({audio: {}});
      document.body.appendChild(pepc);

      const status = await navigator.permissions.query({name: 'microphone'});

      if (status.state !== 'prompt') {
        // If the permission state is already 'granted' or 'denied', the PEPC
        // <usermedia> element is not active (either in no-op or disabled state)
        // and clicking it will not trigger a prompt. We bypass the click and
        // verify the aggregate state using standard getUserMedia instead.
        try {
          const stream =
              await navigator.mediaDevices.getUserMedia({audio: true});
          return (stream.getAudioTracks().length > 0) ? 'SUCCESS' : 'FAIL';
        } catch (err) {
          return 'FAIL';
        }
      }

      // Wait for two animation frames to ensure the PEPC element is fully laid
      // out and rendered by the browser before we simulate the click.
      // Programmatic clicks on unrendered elements are ignored by Blink's
      // security engine.
      await new Promise((resolve) => {
        window.requestAnimationFrame(() => {
          window.requestAnimationFrame(resolve);
        });
      });

      const promise = new Promise((resolve) => {
        pepc.addEventListener('promptaction', () => {
          resolve(pepc.matches(':granted') ? 'SUCCESS' : 'FAIL');
        });
      });

      pepc.click();
      return await promise;
    })();
  )";
  test_case.permission_name = "media";
  test_case.policy_features.insert(
      {network::mojom::PermissionsPolicyFeature::kMicrophone});
  test_case.content_settings_type.insert(
      {ContentSettingsType::MEDIASTREAM_MIC});

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

INSTANTIATE_TEST_SUITE_P(
    ControlledFramePEPC,
    ControlledFramePermissionRequestPEPCTest,
    testing::ValuesIn(GetDefaultPermissionRequestTestParams()),
    [](const testing::TestParamInfo<PermissionRequestTestParam>& info) {
      return info.param.name;
    });

class ControlledFrameUnattachedGuestPermissionRequestTest
    : public ControlledFrameTestBase {
 public:
  void SetUpOnMainThread() override {
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        GetChromeTestDataDir().AppendASCII("web_apps/simple_isolated_app"));
    ControlledFrameTestBase::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(ControlledFrameUnattachedGuestPermissionRequestTest,
                       UnattachedNewWindowGuestDeniesPolicyGatedPermissions) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt,
          "/controlled_frame.html");

  // Trigger window.open and prevent default in newwindow event.
  // This keeps the guest unattached.
  auto test_script = content::JsReplace(
      R"(
(async function() {
  return new Promise((resolve) => {
    const frame = document.getElementsByTagName('controlledframe')[0];
    frame.addEventListener('newwindow', (e) => {
      e.preventDefault();
      resolve('SUCCESS');
    });
    frame.executeScript({code: 'window.open($1);'});
  });
})();
      )",
      embedded_https_test_server().GetURL("/index.html"));

  ASSERT_EQ("SUCCESS", content::EvalJs(app_frame, test_script));

  // Find the unattached guest in C++.
  content::BrowserContext* browser_context = app_frame->GetBrowserContext();
  guest_view::GuestViewManager* manager =
      guest_view::GuestViewManager::FromBrowserContext(browser_context);
  ASSERT_TRUE(manager);

  content::WebContents* guest_contents = nullptr;
  content::WebContents* owner_contents =
      content::WebContents::FromRenderFrameHost(app_frame);
  manager->ForEachUnattachedGuestContents(
      owner_contents,
      [&guest_contents](content::WebContents* unattached_contents) {
        guest_contents = unattached_contents;
      });
  ASSERT_TRUE(guest_contents);

  auto* permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHost(
          guest_contents->GetPrimaryMainFrame());
  ASSERT_TRUE(permission_helper);

  GURL requesting_frame_url("https://attacker.test");
  url::Origin requesting_origin = url::Origin::Create(requesting_frame_url);

  // Test Geolocation
  {
    base::test::TestFuture<bool> future;
    permission_helper->RequestGeolocationPermission(
        requesting_frame_url, /*user_gesture=*/true, future.GetCallback());
    EXPECT_FALSE(future.Get());
  }

  // Test HID
  {
    base::test::TestFuture<bool> future;
    permission_helper->RequestHidPermission(requesting_frame_url,
                                            future.GetCallback());
    EXPECT_FALSE(future.Get());
  }

  // Test Fullscreen
  {
    base::test::TestFuture<bool, const std::string&> future;
    permission_helper->RequestFullscreenPermission(requesting_origin,
                                                   future.GetCallback());
    auto [allowed, user_input] = future.Get();
    EXPECT_FALSE(allowed);
  }

  // Test Clipboard Read/Write
  {
    base::test::TestFuture<bool> future;
    permission_helper->RequestClipboardReadWritePermission(
        requesting_frame_url, /*user_gesture=*/true, future.GetCallback());
    EXPECT_FALSE(future.Get());
  }

  // Test Clipboard Sanitized Write
  {
    base::test::TestFuture<bool> future;
    permission_helper->RequestClipboardSanitizedWritePermission(
        requesting_frame_url, future.GetCallback());
    EXPECT_FALSE(future.Get());
  }

  // Test Media (Camera)
  {
    base::test::TestFuture<bool> future;
    permission_helper->RequestMediaPermission(
        ContentSettingsType::MEDIASTREAM_CAMERA, requesting_frame_url,
        /*user_gesture=*/true, future.GetCallback());
    EXPECT_FALSE(future.Get());
  }

  // Test Media (Microphone)
  {
    base::test::TestFuture<bool> future;
    permission_helper->RequestMediaPermission(
        ContentSettingsType::MEDIASTREAM_MIC, requesting_frame_url,
        /*user_gesture=*/true, future.GetCallback());
    EXPECT_FALSE(future.Get());
  }
}

}  // namespace controlled_frame

namespace controlled_frame {

class ControlledFrameMediaPermissionCacheBrowserTest
    : public ControlledFrameTestBase {
 protected:
  void SetPermission(const GURL& url,
                     ContentSettingsType type,
                     ContentSetting setting) {
    HostContentSettingsMapFactory::GetForProfile(profile())
        ->SetContentSettingDefaultScope(url, url, type, setting);
  }

  std::string QueryPermission(content::RenderFrameHost* frame,
                              const std::string& name) {
    return content::EvalJs(frame, content::JsReplace(R"(
      navigator.permissions.query({name: $1}).then(r => r.state);
    )",
                                                     name))
        .ExtractString();
  }

  std::string RunGetVideo(content::RenderFrameHost* frame) {
    return content::EvalJs(frame, R"(
      (async function() {
        try {
          const stream =
              await navigator.mediaDevices.getUserMedia({video: true});
          const success = stream.getVideoTracks().length > 0;
          stream.getTracks().forEach(t => t.stop());
          return success ? 'SUCCESS' : 'FAIL';
        } catch (err) {
          return 'FAIL: ' + err.name;
        }
      })();
    )")
        .ExtractString();
  }
};

IN_PROC_BROWSER_TEST_F(ControlledFrameMediaPermissionCacheBrowserTest,
                       ClearBrowsingDataClearsCache) {
  GURL guest_url =
      embedded_https_test_server().GetURL("guest.com", "/empty.html");
  url::Origin guest_origin = url::Origin::Create(guest_url);

  // Set profile-wide permissions for the guest origin.
  SetPermission(guest_url, ContentSettingsType::MEDIASTREAM_CAMERA,
                CONTENT_SETTING_ALLOW);

  // Install and open IWA, then create ControlledFrame pointing to the guest
  // origin.
  web_app::ManifestBuilder manifest_builder = web_app::ManifestBuilder();
  manifest_builder.AddPermissionsPolicyWildcard(
      network::mojom::PermissionsPolicyFeature::kCamera);

  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/"guest.com", "/empty.html",
          manifest_builder);
  ASSERT_TRUE(app_frame);
  ASSERT_TRUE(controlled_frame);
  ASSERT_EQ(controlled_frame->GetLastCommittedOrigin(), guest_origin);

  // Set profile-wide permissions for the embedder IWA origin.
  SetPermission(app_frame->GetLastCommittedOrigin().GetURL(),
                ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  // Setup the embedder listener to approve unless should_deny is true.
  EXPECT_TRUE(content::ExecJs(app_frame, R"(
    window.should_deny = false;
    const cf = document.querySelector('controlledframe');
    cf.addEventListener('permissionrequest', (e) => {
      if (window.should_deny) {
        e.request.deny();
      } else {
        e.request.allow();
      }
    });
  )"));

  // The first request will succeed because the embedder approves it and the
  // content setting is ALLOW.
  EXPECT_EQ("SUCCESS", RunGetVideo(controlled_frame));

  // Clear the content settings for Camera (simulating Clear Browsing Data)
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->ClearSettingsForOneType(ContentSettingsType::MEDIASTREAM_CAMERA);

  // Now change the embedder's approval logic to deny requests to verify that
  // the cache was cleared and it falls back to the embedder listener.
  EXPECT_TRUE(content::ExecJs(app_frame, R"(
    window.should_deny = true;
  )"));

  // This request should now fail because the cache was wiped, and the
  // embedder listener now explicitly denies the request. If the cache was not
  // wiped, this would incorrectly return SUCCESS.
  EXPECT_EQ("FAIL: NotAllowedError", RunGetVideo(controlled_frame));
}

}  // namespace controlled_frame
