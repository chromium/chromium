// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/extension_features.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StartsWith;
using testing::UnorderedElementsAre;

namespace controlled_frame {

class ControlledFrameNewWindowBrowserTest
    : public ControlledFrameTestBase,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        GetChromeTestDataDir().AppendASCII("web_apps/simple_isolated_app"));
    ControlledFrameTestBase::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(ControlledFrameNewWindowBrowserTest, AttachSucceeds) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt,
          "/controlled_frame.html");

  auto test_script = content::JsReplace(
      R"(
(async function() {
  try {
    await new Promise((resolve, reject) => {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame) {
        reject('Could not find a controlledframe element.');
      }
      frame.addEventListener('newwindow', (e) => {
        if (!e.window.attach) {
          reject('window.attach does not exist ');
        }
        const newcontrolledframe = document.createElement('controlledframe');
        // Attach the new window to the new <controlledframe>.
        try {
          newcontrolledframe.addEventListener(
              'loadstop', resolve);
          newcontrolledframe.addEventListener(
              'loadabort', reject);
          e.window.attach(newcontrolledframe);
          document.body.appendChild(newcontrolledframe);
        } catch (err) {
          reject(err.message);
        }
      });
      frame.executeScript({code: 'window.open($1);'});
    });

    const frames = document.getElementsByTagName('controlledframe');
    if (frames.length !== 2) {
      return [
        'FAIL: expected 2 <controlledframe> elements, found ' + frames.length
      ];
    }

    async function getCurrentLocationOfControlledFrame(frame) {
      const result = await frame.executeScript({code: 'window.location.href;'});
      if (!result) {
        return 'FAIL: executeScript() returned no result';
      }
      return result[0];
    };

    return [
      await getCurrentLocationOfControlledFrame(frames[0]),
      await getCurrentLocationOfControlledFrame(frames[1]),
    ];
  } catch (e) {
    return ['FAIL: ' + e.message];
  }
})();
    )",
      embedded_https_test_server().GetURL("/index.html"));

  EXPECT_THAT(
      content::EvalJs(app_frame, test_script).TakeValue().TakeList(),
      UnorderedElementsAre(
          embedded_https_test_server().GetURL("/controlled_frame.html").spec(),
          embedded_https_test_server().GetURL("/index.html").spec()));
}

IN_PROC_BROWSER_TEST_F(ControlledFrameNewWindowBrowserTest, DiscardSucceeds) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt,
          "/controlled_frame.html");

  std::string test_script =
      content::JsReplace(R"(
(async function() {
  try {
    return await new Promise ((resolve) => {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame) {
        resolve('FAIL: Could not find a controlledframe element.');
      }
      frame.addEventListener('newwindow', (e) => {
        try {
          e.window.discard();
          resolve('SUCCESS');
        } catch (err) {
          resolve('FAIL: ' + err.message);
        }
      });
      frame.executeScript({code: 'window.open($1);'});
    });
  } catch (err) {
    return "FAIL: " + err.message;
  }
})();
    )",
                         embedded_https_test_server().GetURL("/index.html"));

  ASSERT_EQ("SUCCESS", content::EvalJs(app_frame, test_script));

  EXPECT_EQ(1, content::EvalJs(
                   app_frame,
                   "document.getElementsByTagName('controlledframe').length;"));
}

IN_PROC_BROWSER_TEST_F(ControlledFrameNewWindowBrowserTest,
                       PostMessageAfterAttachSucceeds) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt,
          "/controlled_frame.html");

  auto test_script = content::JsReplace(
      R"(
async function executeScriptOnFrame(frame, script) {
  const result = await frame.executeScript({code: script});
  if (!result) {
    throw new Error('executeScript returned no result');
  }
  if (result[0] !== 'SUCCESS') {
    throw new Error('expected SUCCESS but got ' + result[0]);
  }
  return 'SUCCESS';
};

(async function() {
  try {
    await new Promise((resolve, reject) => {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame) {
        reject('Could not find a controlledframe element.');
      }

      frame.addEventListener('newwindow', (e) => {
        if (!e.window.attach) {
          reject('window.attach does not exist ');
        }
        const newcontrolledframe = document.createElement('controlledframe');
        // Attach the new window to the new <controlledframe>.
        try {
          newcontrolledframe.addEventListener('loadstop', resolve);
          newcontrolledframe.addEventListener('loadabort', reject);
          e.window.attach(newcontrolledframe);
          document.body.appendChild(newcontrolledframe);
        } catch (err) {
          reject(err.message);
        }
      });
      frame.executeScript({code: 'document.openedWindow = window.open($1);'});
    });

    const listenscript = `
      (function() {
        window.addEventListener('message', (e) => {document.lastMessage = e;});
        return 'SUCCESS';
      })();
    `;
    const sendscript = `
      (function() {
        const target = document.openedWindow || window.opener;
        if (target === null) {
          return 'missing postMessage target';
        }
        target.postMessage('hello test');
        return 'SUCCESS';
      })();
    `;
    const verifyscript = `
      (function() {
        if (!document.lastMessage) {
          return 'no message received';
        }
        if (document.lastMessage.data !== 'hello test') {
          return 'unexpected lastMessage\\nexpected: hello test\\nactual: ' +
              document.lastMessage.data;
        }
        return 'SUCCESS';
      })();
    `;

    const frames = Array.from(document.getElementsByTagName('controlledframe'));
    if (frames.length !== 2) {
      throw new Error(
          'expected 2 <controlledframe> elements, found ' + frames.length);
    }
    for (const frame of frames) {
      await executeScriptOnFrame(frame, listenscript);
    }
    for (const frame of frames) {
      await executeScriptOnFrame(frame, sendscript);
    }
    // Trigger resolve() in 0.1s after sending out messages.
    await new Promise((resolve) => {
      setTimeout(resolve, 100);
    });
    for (const frame of frames) {
      await executeScriptOnFrame(frame, verifyscript);
    }
    return 'SUCCESS';
  } catch (e) {
    return 'FAIL: ' + e.message;
  }
})();
    )",
      embedded_https_test_server().GetURL("/index.html"));

  EXPECT_EQ("SUCCESS", content::EvalJs(app_frame, test_script));
}

class ControlledFrameNewWindowHidBrowserTest
    : public ControlledFrameNewWindowBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ControlledFrameNewWindowBrowserTest::SetUpOnMainThread();

    guest_view_manager_ = factory_.GetOrCreateTestGuestViewManager(
        profile(), extensions::ExtensionsAPIClient::Get()
                       ->CreateGuestViewManagerDelegate());

    mojo::PendingRemote<device::mojom::HidManager> pending_remote;
    hid_manager_.Bind(pending_remote.InitWithNewPipeAndPassReceiver());
    base::test::TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>>
        devices_future;
    auto* chooser_context = HidChooserContextFactory::GetForProfile(profile());
    chooser_context->SetHidManagerForTesting(std::move(pending_remote),
                                             devices_future.GetCallback());
    ASSERT_TRUE(devices_future.Wait());
  }

  void TearDownOnMainThread() override {
    guest_view_manager_ = nullptr;
    ControlledFrameNewWindowBrowserTest::TearDownOnMainThread();
  }

  guest_view::TestGuestViewManager* guest_view_manager() {
    return guest_view_manager_;
  }

  device::FakeHidManager& hid_manager() { return hid_manager_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      extensions_features::kEnableWebHidInWebView};
  guest_view::TestGuestViewManagerFactory factory_;
  raw_ptr<guest_view::TestGuestViewManager> guest_view_manager_ = nullptr;
  device::FakeHidManager hid_manager_;
};

IN_PROC_BROWSER_TEST_F(ControlledFrameNewWindowHidBrowserTest,
                       UnattachedGuestUsesWebViewPermissionStore) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt,
          "/controlled_frame.html");
  ASSERT_TRUE(app_frame);
  ASSERT_TRUE(controlled_frame);

  // Connect a fake device and grant the guest's origin profile-level access to
  // it, as if the same origin had been granted access from a top-level tab.
  device::mojom::HidDeviceInfoPtr device = hid_manager().CreateAndAddDevice(
      "physical-id", /*vendor_id=*/0x1234, /*product_id=*/0xabcd,
      "Test HID Device", "serial", device::mojom::HidBusType::kHIDBusTypeUSB);
  auto* chooser_context = HidChooserContextFactory::GetForProfile(profile());
  const url::Origin guest_origin = controlled_frame->GetLastCommittedOrigin();
  chooser_context->GrantDevicePermission(guest_origin, *device);
  ASSERT_TRUE(chooser_context->HasDevicePermission(guest_origin, *device));

  // Open a new window from the guest and leave it unattached.
  ASSERT_TRUE(content::ExecJs(app_frame, R"(
    const frame = document.getElementsByTagName('controlledframe')[0];
    frame.addEventListener('newwindow', (e) => {
      e.preventDefault();
      window.pendingNewWindow = e.window;
    });
    frame.executeScript({code: 'window.open();'});
  )"));
  guest_view_manager()->WaitForNumGuestsCreated(2u);

  guest_view::GuestViewBase* new_guest =
      guest_view_manager()->GetLastGuestViewCreated();
  ASSERT_TRUE(new_guest);
  ASSERT_FALSE(new_guest->attached());
  content::RenderFrameHost* new_guest_rfh = new_guest->GetGuestMainFrame();
  ASSERT_TRUE(new_guest_rfh);
  ASSERT_TRUE(extensions::WebViewGuest::FromRenderFrameHost(new_guest_rfh));
  ASSERT_EQ(guest_origin, new_guest_rfh->GetLastCommittedOrigin());

  // Permission checks for guests must be answered from the per-embedder
  // WebViewChooserContext rather than the profile-level store, so neither the
  // attached opener nor the unattached new window should be granted access.
  ChromeHidDelegate hid_delegate;
  EXPECT_FALSE(hid_delegate.HasDevicePermission(profile(), controlled_frame,
                                                guest_origin, *device));
  EXPECT_FALSE(hid_delegate.HasDevicePermission(profile(), new_guest_rfh,
                                                guest_origin, *device));

  // Revoking on behalf of the unattached guest must not touch the
  // profile-level grant.
  hid_delegate.RevokeDevicePermission(profile(), new_guest_rfh, guest_origin,
                                      *device);
  EXPECT_TRUE(chooser_context->HasDevicePermission(guest_origin, *device));
}

}  // namespace controlled_frame
