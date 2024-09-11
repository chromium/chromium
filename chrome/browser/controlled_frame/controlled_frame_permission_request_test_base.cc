// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "url/origin.h"

using testing::EndsWith;
using testing::StartsWith;

namespace controlled_frame {

namespace {
constexpr char kPermissionAllowedHost[] = "permission-allowed.com";
constexpr char kPermissionDisallowedHost[] = "permission-disllowed.com";

const std::vector<PermissionRequestTestParam> kTestParams = {
    {.name = "Succeeds",
     .calls_allow = true,
     .embedder_policy = EmbedderPolicy::kBothEmbedderAndRequestingOrigin,
     .allowed_by_embedder_content_settings = true,
     .embedded_origin_content_settings_state = ContentSettingsState::kDefault,
     .expected_success = true},
    {.name = "FailsBecauseNotAllow",
     .calls_allow = false,
     .embedder_policy = EmbedderPolicy::kBothEmbedderAndRequestingOrigin,
     .allowed_by_embedder_content_settings = true,
     .embedded_origin_content_settings_state = ContentSettingsState::kDefault,
     .expected_success = false},
    {.name = "FailsBecauseEmbedderDoesNotHavePermissionsPolicy",
     .calls_allow = true,
     .embedder_policy = EmbedderPolicy::kNoPolicy,
     .allowed_by_embedder_content_settings = true,
     .embedded_origin_content_settings_state = ContentSettingsState::kDefault,
     .expected_success = false},
    {.name = "FailsBecauseEmbedderPermissionsPolicyMissingEmbedderOrigin",
     .calls_allow = true,
     .embedder_policy = EmbedderPolicy::kNoEmbedderOrigin,
     .allowed_by_embedder_content_settings = true,
     .embedded_origin_content_settings_state = ContentSettingsState::kDefault,
     .expected_success = false},
    {.name = "FailsBecauseEmbedderPermissionsPolicyMissingRequestingOrigin",
     .calls_allow = true,
     .embedder_policy = EmbedderPolicy::kNoRequestingOrigin,
     .allowed_by_embedder_content_settings = true,
     .embedded_origin_content_settings_state = ContentSettingsState::kDefault,
     .expected_success = false},
    {.name = "FailsBecauseNoEmbedderContentSettings",
     .calls_allow = true,
     .embedder_policy = EmbedderPolicy::kBothEmbedderAndRequestingOrigin,
     .allowed_by_embedder_content_settings = false,
     .embedded_origin_content_settings_state = ContentSettingsState::kDefault,
     .expected_success = false},
    {.name = "SucceedsWhenEmbeddedOriginPermissionStateIsDenied",
     .calls_allow = true,
     .embedder_policy = EmbedderPolicy::kBothEmbedderAndRequestingOrigin,
     .allowed_by_embedder_content_settings = true,
     .embedded_origin_content_settings_state = ContentSettingsState::kDeny,
     .expected_success = true},
    {.name = "FailsWhenEmbeddedOriginPermissionStateIsAllowed",
     .calls_allow = true,
     .embedder_policy = EmbedderPolicy::kBothEmbedderAndRequestingOrigin,
     .allowed_by_embedder_content_settings = false,
     .embedded_origin_content_settings_state = ContentSettingsState::kAllow,
     .expected_success = false},
};

const std::vector<DisabledPermissionTestParam> kDisabledPermissionsTestParams =
    {{.name = "BothFailsWhenPermissionsPolicyIsNotEnabled",
      .policy_features_enabled = false,
      .iwa_expect_success = false,
      .controlled_frame_expect_success = false},
     {.name = "IwaSucceedsButControlledFrameFails",
      .policy_features_enabled = true,
      .iwa_expect_success = true,
      .controlled_frame_expect_success = false}};

class PermissionRequestEventObserver
    : public extensions::EventRouter::TestObserver {
 public:
  void OnWillDispatchEvent(const extensions::Event& event) override {}
  void OnDidDispatchEventToProcess(const extensions::Event& event,
                                   int process_id) override {}

  void OnNonExtensionEventDispatched(const std::string& event_name) override {
    events_.push_back(event_name);
  }

  const std::vector<std::string>& events() const { return events_; }

 private:
  std::vector<std::string> events_;
};

ContentSetting ContentSettingFromState(ContentSettingsState state) {
  CHECK(state != ContentSettingsState::kDefault);
  if (state == ContentSettingsState::kAllow) {
    return ContentSetting::CONTENT_SETTING_ALLOW;
  }
  return ContentSetting::CONTENT_SETTING_BLOCK;
}

void FocusControlledFrame(content::RenderFrameHost* controlled_frame) {
  // Focus <controlledframe> with a fake click.
  content::SimulateMouseClick(
      content::WebContents::FromRenderFrameHost(controlled_frame),
      /*modifiers=*/0, blink::WebMouseEvent::Button::kLeft);
}

}  // namespace

PermissionRequestTestCase::PermissionRequestTestCase() = default;
PermissionRequestTestCase::~PermissionRequestTestCase() = default;

const std::vector<PermissionRequestTestParam>&
GetDefaultPermissionRequestTestParams() {
  return kTestParams;
}

DisabledPermissionTestCase::DisabledPermissionTestCase() = default;
DisabledPermissionTestCase::~DisabledPermissionTestCase() = default;

const std::vector<DisabledPermissionTestParam>&
GetDefaultDisabledPermissionTestParams() {
  return kDisabledPermissionsTestParams;
}

void ControlledFramePermissionRequestTestBase::SetUpOnMainThread() {
  ControlledFrameTestBase::SetUpOnMainThread();
  StartContentServer("web_apps/simple_isolated_app");
}

void ControlledFramePermissionRequestTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  ControlledFrameTestBase::SetUpCommandLine(command_line);
  command_line->AppendArg("--use-fake-device-for-media-stream");
}

void ControlledFramePermissionRequestTestBase::
    SetUpPermissionRequestEventListener(
        content::RenderFrameHost* app_frame,
        const std::string& expected_permission_name,
        bool allow_permission) {
  const std::string handle_request_str = allow_permission ? "allow" : "deny";
  EXPECT_EQ("SUCCESS", content::EvalJs(app_frame, content::JsReplace(
                                                      R"(
      (function() {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame) {
          return 'FAIL: Could not find a controlledframe element.';
        }
        frame.addEventListener('permissionrequest', (e) => {
          if (e.permission === $1) {
            e.request[$2]();
          }
        });
        return 'SUCCESS';
      })();
    )",
                                                      expected_permission_name,
                                                      handle_request_str)));
}

void ControlledFramePermissionRequestTestBase::VerifyEnabledPermission(
    const PermissionRequestTestCase& test_case,
    const PermissionRequestTestParam& test_param,
    std::optional<base::OnceCallback<std::string(bool)>>
        get_expected_result_callback) {
  // If the permission has no dependent permissions policy feature, then skip
  // the true negative permissions policy test cases.
  if (test_param.embedder_policy !=
          EmbedderPolicy::kBothEmbedderAndRequestingOrigin &&
      test_case.policy_features.empty()) {
    return;
  }

  // If the permission has no dependent embedder content setting, then skip
  // the true negative embedder content settings test cases.
  if (!test_param.allowed_by_embedder_content_settings &&
      test_case.content_settings_type.empty()) {
    return;
  }

  web_app::ManifestBuilder manifest_builder = web_app::ManifestBuilder();

  if (test_param.embedder_policy != EmbedderPolicy::kNoPolicy) {
    url::Origin policy_origin = embedded_https_test_server().GetOrigin(
        test_param.embedder_policy == EmbedderPolicy::kNoRequestingOrigin
            ? kPermissionDisallowedHost
            : kPermissionAllowedHost);

    for (auto& policy_feature : test_case.policy_features) {
      manifest_builder.AddPermissionsPolicy(
          policy_feature,
          test_param.embedder_policy != EmbedderPolicy::kNoEmbedderOrigin,
          {policy_origin});
    }
  }

  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/kPermissionAllowedHost,
          /*controlled_frame_src_relative_url=*/"/index.html",
          manifest_builder);

  FocusControlledFrame(controlled_frame);

  SetUpPermissionRequestEventListener(app_frame, test_case.permission_name,
                                      test_param.calls_allow);

  for (const auto& content_settings_type : test_case.content_settings_type) {
    HostContentSettingsMapFactory::GetForProfile(profile())
        ->SetContentSettingDefaultScope(
            app_frame->GetLastCommittedOrigin().GetURL(),
            app_frame->GetLastCommittedOrigin().GetURL(), content_settings_type,
            test_param.allowed_by_embedder_content_settings
                ? ContentSetting::CONTENT_SETTING_ALLOW
                : ContentSetting::CONTENT_SETTING_BLOCK);

    if (test_param.embedded_origin_content_settings_state !=
        ContentSettingsState::kDefault) {
      HostContentSettingsMapFactory::GetForProfile(profile())
          ->SetContentSettingDefaultScope(
              controlled_frame->GetLastCommittedOrigin().GetURL(),
              controlled_frame->GetLastCommittedOrigin().GetURL(),
              content_settings_type,
              ContentSettingFromState(
                  test_param.embedded_origin_content_settings_state));
    }
  }

  PermissionRequestEventObserver event_observer;
  extensions::EventRouter::Get(profile())->AddObserverForTesting(
      &event_observer);

  base::OnceCallback<std::string(bool)> callback =
      get_expected_result_callback.has_value()
          ? std::move(get_expected_result_callback.value())
          : base::BindLambdaForTesting([](bool should_success) -> std::string {
              return should_success ? "SUCCESS" : "FAIL";
            });

  EXPECT_THAT(
      content::EvalJs(controlled_frame, test_case.test_script).ExtractString(),
      StartsWith(std::move(callback).Run(test_param.expected_success)));

  // The permission event should only be fired when the embedder has the
  // permissions policy for both the embedder and the requesting origin.
  bool should_fire_permission_event =
      test_param.embedder_policy ==
      EmbedderPolicy::kBothEmbedderAndRequestingOrigin;
  EXPECT_EQ(event_observer.events().size(),
            should_fire_permission_event ? 1ul : 0ul);

  if (event_observer.events().size()) {
    EXPECT_THAT(event_observer.events().back(),
                EndsWith("onPermissionRequest"));
  }

  extensions::EventRouter::Get(profile())->RemoveObserverForTesting(
      &event_observer);
}

void ControlledFramePermissionRequestTestBase::VerifyDisabledPermission(
    const DisabledPermissionTestCase& test_case,
    const DisabledPermissionTestParam& test_param) {
  auto [app_frame, controlled_frame] =
      SetUpControlledFrame(test_case, test_param);
  if (!app_frame || !controlled_frame) {
    return;
  }
  VerifyDisabledPermission(test_case, test_param, app_frame, controlled_frame);
}

void ControlledFramePermissionRequestTestBase::VerifyDisabledPermission(
    const DisabledPermissionTestCase& test_case,
    const DisabledPermissionTestParam& test_param,
    content::RenderFrameHost* app_frame,
    content::RenderFrameHost* controlled_frame) {
  std::string expected_iwa_result = test_param.iwa_expect_success
                                        ? test_case.success_result
                                        : test_case.failure_result;
  std::string expected_cf_result = test_param.controlled_frame_expect_success
                                       ? test_case.success_result
                                       : test_case.failure_result;

  EXPECT_THAT(content::EvalJs(app_frame, test_case.request_script),
              expected_iwa_result);

  FocusControlledFrame(controlled_frame);

  ASSERT_EQ("SUCCESS", content::EvalJs(app_frame,
                                       R"(
(function() {
  const frame = document.getElementsByTagName('controlledframe')[0];
  if (!frame) {
    return 'FAIL: Could not find a controlledframe element.';
  }
  window.permissionEventHandled = false;
  frame.addEventListener('permissionrequest', (e) => {
    window.permissionEventHandled = true;
  });
  return 'SUCCESS';
})();
    )"));

  EXPECT_THAT(content::EvalJs(controlled_frame, test_case.request_script),
              expected_cf_result);

  EXPECT_EQ(content::EvalJs(app_frame, "window.permissionEventHandled;"),
            false);
}

std::pair<content::RenderFrameHost*, content::RenderFrameHost*>
ControlledFramePermissionRequestTestBase::SetUpControlledFrame(
    const DisabledPermissionTestCase& test_case,
    const DisabledPermissionTestParam& test_param) {
  // If the permission has no dependent permissions policy feature, then
  // skip
  // the true negative permissions policy test cases.
  if (!test_param.policy_features_enabled &&
      test_case.policy_features.empty()) {
    return {nullptr, nullptr};
  }

  web_app::ManifestBuilder manifest_builder = web_app::ManifestBuilder();
  if (test_param.policy_features_enabled) {
    for (auto& policy_feature : test_case.policy_features) {
      manifest_builder.AddPermissionsPolicyWildcard(policy_feature);
    }
  }

  return InstallAndOpenIwaThenCreateControlledFrame(
      /*controlled_frame_host_name=*/kPermissionAllowedHost,
      /*controlled_frame_src_relative_url=*/"/index.html", manifest_builder);
}

}  // namespace controlled_frame
