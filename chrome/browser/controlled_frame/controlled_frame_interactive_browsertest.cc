// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace controlled_frame {

class ControlledFramePermissionRequestInteractiveTest
    : public ControlledFramePermissionRequestTestBase,
      public testing::WithParamInterface<PermissionRequestTestParam> {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      permissions::features::kKeyboardAndPointerLockPrompt};
};

// Pointer lock & Fullscreen are not available on MacOS bots.
#if !BUILDFLAG(IS_MAC)

// This is an interactive_ui_test because pointer locks affect global system
// state, which could interact poorly with other concurrently run tests.
IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestInteractiveTest,
                       PointerLock) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      try {
        await document.body.requestPointerLock();
        if (document.pointerLockElement !== document.body) {
          return 'FAIL: pointer did not lock, or locked to wrong element';
        }
        return 'SUCCESS';
      } catch (err) {
        return `FAIL: ${err.name}: ${err.message}`;
      }
    })();
  )";
  test_case.permission_name = "pointerLock";
  test_case.content_settings_type.insert(ContentSettingsType::POINTER_LOCK);

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestInteractiveTest,
                       Fullscreen) {
  PermissionRequestTestCase test_case;
  test_case.test_script = R"(
    (async function() {
      try {
        if (document.fullscreenElement){
          return 'FAIL: Already fullscreen';
        }
        document.body.requestFullscreen();
        // Wait for 2 seconds;
        await new Promise(resolve => setTimeout(resolve, 2000));
        return (document.fullscreenElement  === document.body) ?
               'SUCCESS' : 'FAIL: document.body is not fullscreen';
      } catch (err) {
        return 'FAIL: ${err.name}: ${err.message}';
      }
    })();
  )";
  test_case.permission_name = "fullscreen";
  test_case.policy_features.insert(
      {blink::mojom::PermissionsPolicyFeature::kFullscreen});

  PermissionRequestTestParam test_param = GetParam();
  VerifyEnabledPermission(test_case, test_param);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/
                         ,
                         ControlledFramePermissionRequestInteractiveTest,
                         testing::ValuesIn(
                             GetDefaultPermissionRequestTestParams()),
                         [](const testing::TestParamInfo<
                             PermissionRequestTestParam>& info) {
                           return info.param.name;
                         });

class ControlledFramePointerLockInteractiveUiTest
    : public ControlledFrameTestBase {
 public:
  void SetUpOnMainThread() override {
    ControlledFrameTestBase::SetUpOnMainThread();
    StartContentServer("web_apps/simple_isolated_app");
  }

 protected:
  void AllowOnlyPointerLockPermission(content::RenderFrameHost* app_frame) {
    HostContentSettingsMapFactory::GetForProfile(profile())
        ->SetContentSettingDefaultScope(
            app_frame->GetLastCommittedOrigin().GetURL(),
            app_frame->GetLastCommittedOrigin().GetURL(),
            ContentSettingsType::POINTER_LOCK,
            ContentSetting::CONTENT_SETTING_ALLOW);

    ASSERT_TRUE(content::ExecJs(app_frame, R"(
      const cf = document.querySelector('controlledframe');
      cf.addEventListener('permissionrequest', (e) => {
        if (e.permission === 'pointerLock') {
          e.request.allow();
        } else {
          e.request.deny();
        }
      });
    )"));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      permissions::features::kKeyboardAndPointerLockPrompt};
};

IN_PROC_BROWSER_TEST_F(ControlledFramePointerLockInteractiveUiTest,
                       EscapeExitsPointerLock) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt, "/index.html");
  AllowOnlyPointerLockPermission(app_frame);

  ASSERT_TRUE(content::ExecJs(controlled_frame, R"(
    new Promise((resolve, reject) => {
      window.changeListener = document.addEventListener(
          'pointerlockchange', () => {
            if (document.pointerLockElement === document.body) {
              resolve();
            } else {
              reject();
            }
          }, {once: true});
      window.errorListener = document.addEventListener(
          'pointerlockerror', (e) => {
            reject(e);
          }, {once: true});

      document.body.requestPointerLock();
    });
  )"));

  ASSERT_TRUE(content::ExecJs(controlled_frame, R"(
    document.removeEventListener('pointerlockchange', window.changeListener);
    document.removeEventListener('pointerlockerror', window.errorListener);

    window.releasePromise = new Promise((resolve, reject) => {
      document.addEventListener('pointerlockchange', () => {
        if (document.pointerLockElement === null) {
          resolve();
        } else {
          reject();
        }
      }, {once: true});
      document.addEventListener('pointerlockerror', (e) => {
        reject(e);
      }, {once: true});
    });
  )",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // At this point, `releasePromise` has defined new event listeners.
  // These are waiting for the pointer lock to be released, which we will
  // trigger using the Escape key signal next.
  Browser* app_window = chrome::FindBrowserWithTab(
      content::WebContents::FromRenderFrameHost(app_frame));
  ASSERT_TRUE(app_window);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(app_window, ui::VKEY_ESCAPE,
                                              false, false, false, false));

  ASSERT_TRUE(content::ExecJs(controlled_frame, "window.releasePromise"));
}

#endif  // !BUILDFLAG(IS_MAC)

}  // namespace controlled_frame
