// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_converters.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_coordinator.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "ui/gfx/geometry/rect.h"
#endif

namespace glic {

#if !BUILDFLAG(IS_ANDROID)
using GlicExperimentalTriggeringBrowserTest = GlicBrowserTest;

IN_PROC_BROWSER_TEST_F(
    GlicExperimentalTriggeringBrowserTest,
    ExperimentalTriggeringWithFocusedPwaDoesNotCreateNewWindow) {
  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;
  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(
      GetProfile(), /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  glic_service->enabling().SetUserEnabledActuationOnWeb(true);
  glic_service->enabling().SetExperimentalTriggeringEnabled(true);

  // 1. Create and show a PWA app window.
  BrowserWindowInterface* app_browser =
      CreateBrowserWindow(BrowserWindowCreateParams::CreateForApp(
          "test_app", /*trusted_source=*/true, gfx::Rect(), GetProfile(),
          /*user_gesture=*/true));
  app_browser->GetWindow()->Show();

  // 2. Experimental triggering request fires while PWA window is focused.
  GlicExperimentalTriggeringCoordinator coordinator(GetProfile());
  ExperimentalTriggeringRequest request;
  request.version = 1;
  request.context_id = "test_context";
  request.task_metadata = TaskMetadata{.conversation_id = "conv_123"};
  request.payload = TriggerActuationRequest{.initial_prompt = "hello"};

  coordinator.OnRequest(
      "test_context", request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(GetProfile()),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      base::DoNothing(), nullptr);

  // 3. Verify no new normal browser window was created.
  size_t normal_browser_count = 0;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* b) {
        if (b->GetProfile() == GetProfile() &&
            b->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
          normal_browser_count++;
        }
        return true;
      });
  EXPECT_EQ(1u, normal_browser_count);

  CloseBrowserSynchronously(app_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;

void OptInProfileForExperimentalTriggering(Profile* profile) {
  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile, /*create=*/true);
  ASSERT_TRUE(glic_service);
  profile->GetPrefs()->SetInteger(
      glic::prefs::kGlicSparkPolicySettings,
      static_cast<int>(glic::prefs::GlicSparkPolicyState::kEnabled));
  glic_service->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  glic_service->enabling().SetUserEnabledActuationOnWeb(true);
  glic_service->enabling().SetExperimentalTriggeringEnabled(true);
}

components_sharing_message::GlicExperimentalTriggering
CreateTriggerActuationProto(const std::string& context_id) {
  components_sharing_message::GlicExperimentalTriggering proto;
  proto.set_context_id(context_id);
  proto.set_glic_experimental_triggering_version(
      GlicEnabling::kExperimentalTriggeringProtocolVersion);
  proto.mutable_task_metadata()->set_sender_sequence_number(1);
  proto.mutable_task_metadata()->set_conversation_id("conv_123");
  proto.mutable_request()
      ->mutable_trigger_actuation_request()
      ->set_initial_prompt("hello");
  return proto;
}

}  // namespace

// End-to-end integration test driving `GlicExperimentalTriggeringCoordinator`
// through the real `GlicKeyedService`, `GlicInvokeHandler`, `Host`, and
// `chrome://glic` WebUI (`glic_app_controller.ts`) when offline.
class GlicExperimentalTriggeringOfflineWebUiBrowserTest
    : public GlicBrowserTest {
 public:
  GlicExperimentalTriggeringOfflineWebUiBrowserTest() {
    GlicUI::simulate_no_connection_for_testing();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActor,
                              features::kGlicExperimentalTriggering,
                              features::kGlicActorPolicyControlExemption},
        /*disabled_features=*/{::features::kGlicIgnoreOfflineState,
                               features::kGlicNoWebview});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    GlicExperimentalTriggeringOfflineWebUiBrowserTest,
    ForegroundPreparedTabOfflineFailurePopulatesPanelOpenAndLoadErrorMetadata) {
  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;
  OptInProfileForExperimentalTriggering(GetProfile());

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab);

  GlicExperimentalTriggeringCoordinator coordinator(GetProfile());
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  ExperimentalTriggeringRequest domain_request =
      ProtoToRequest(CreateTriggerActuationProto("test_context"));
  domain_request.context_id = "test_context";
  auto start_response = coordinator.OnRequest(
      "test_context", domain_request,
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(GetProfile()),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(), active_tab);
  ASSERT_TRUE(start_response.has_value());
  ASSERT_TRUE(start_response->task_update.has_value());
  EXPECT_EQ(start_response->task_update->state, TaskUpdate::State::kStarting);
  EXPECT_TRUE(start_response->task_update->metadata.empty());

  ExperimentalTriggeringResponse failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data,
            "Glic invocation failed: Client load error");

  const auto& metadata = failure.task_update->metadata;
  EXPECT_THAT(metadata, IsSupersetOf({
                            Pair("chrome_error_code", "GLIC_CLIENT_LOAD_ERROR"),
                            Pair("panel_opened", "1"),
                            Pair("client_connected", "0"),
                        }));
  EXPECT_THAT(metadata,
              Contains(Pair("chrome_invoke_diagnostics", Not(IsEmpty()))));
  EXPECT_THAT(metadata, Not(Contains(Key("t_to_client_connected_ms"))));
  EXPECT_THAT(metadata, Not(Contains(Key("t_to_first_response_ms"))));

  ASSERT_THAT(metadata, Contains(Key("t_to_panel_open_ms")));
  ASSERT_THAT(metadata, Contains(Key("t_to_terminal_ms")));
  int64_t panel_open_ms = -1;
  int64_t terminal_ms = -1;
  EXPECT_TRUE(
      base::StringToInt64(metadata.at("t_to_panel_open_ms"), &panel_open_ms));
  EXPECT_TRUE(
      base::StringToInt64(metadata.at("t_to_terminal_ms"), &terminal_ms));
  EXPECT_GE(panel_open_ms, 0);
  EXPECT_GE(terminal_ms, panel_open_ms);
}

// End-to-end integration test driving `GlicExperimentalTriggeringCoordinator`
// through the real `GlicInvokeHandler` and `chrome://glic` WebUI on a
// background tab whose panel is hidden (`!instance->IsShowing()`), verifying
// that a hidden sign-in load failure fails immediately with
// `GLIC_CLIENT_LOAD_ERROR` and `panel_opened = "0"` rather than stalling on
// the 5-minute invocation timeout.
class GlicExperimentalTriggeringSignInWebUiBrowserTest
    : public GlicBrowserTest {
 public:
  GlicExperimentalTriggeringSignInWebUiBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActor,
                              features::kGlicExperimentalTriggering,
                              features::kGlicActorPolicyControlExemption},
        /*disabled_features=*/{features::kGlicNoWebview,
                               ::features::kGlicBackgroundTriggering});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    GlicExperimentalTriggeringSignInWebUiBrowserTest,
    BackgroundTabHiddenSignInFailureFailsImmediatelyWithPanelClosedMetadata) {
  OptInProfileForExperimentalTriggering(GetProfile());
  InvalidateAccount(GetProfile());
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(GetProfile()),
            mojom::ProfileReadyState::kSignInRequired);

  auto* tab_list = GetTabListInterface();
  const int initial_tab_count = tab_list->GetTabCount();

  GlicExperimentalTriggeringCoordinator coordinator(GetProfile());
  base::test::TestFuture<ExperimentalTriggeringResponse> update_future;
  base::test::TestFuture<std::optional<ExperimentalTriggeringResponse>>
      start_response_future;
  coordinator.OnProtoMessage(
      "test_context", CreateTriggerActuationProto("test_context"),
      ScopedIncomingMessageResultLogger(
          GlicMetricReporter(GetProfile()),
          ScopedIncomingMessageResultLogger::Channel::kSharingMessage),
      update_future.GetRepeatingCallback(),
      start_response_future.GetCallback());
  std::optional<ExperimentalTriggeringResponse> start_response =
      start_response_future.Take();
  ASSERT_TRUE(start_response.has_value());
  ASSERT_TRUE(start_response->task_update.has_value());
  EXPECT_EQ(start_response->task_update->state, TaskUpdate::State::kStarting);

  ExperimentalTriggeringResponse failure = update_future.Take();
  ASSERT_TRUE(failure.task_update.has_value());
  EXPECT_EQ(failure.task_update->state, TaskUpdate::State::kFailed);
  EXPECT_EQ(failure.task_update->data,
            "Glic invocation failed: Client load error");

  ASSERT_EQ(tab_list->GetTabCount(), initial_tab_count + 1);
  tabs::TabInterface* background_tab = tab_list->GetTab(initial_tab_count);
  ASSERT_TRUE(background_tab);
  EXPECT_FALSE(background_tab->IsActivated());

  GlicInstance* instance =
      GlicKeyedService::Get(GetProfile())->GetInstanceForTab(background_tab);
  ASSERT_TRUE(instance);
  EXPECT_FALSE(instance->IsShowing());

  const auto& metadata = failure.task_update->metadata;
  EXPECT_THAT(metadata, IsSupersetOf({
                            Pair("chrome_error_code", "GLIC_CLIENT_LOAD_ERROR"),
                            Pair("panel_opened", "0"),
                            Pair("client_connected", "0"),
                        }));
  EXPECT_THAT(metadata,
              Contains(Pair("chrome_invoke_diagnostics", Not(IsEmpty()))));
  EXPECT_THAT(metadata, Not(Contains(Key("t_to_panel_open_ms"))));
  EXPECT_THAT(metadata, Not(Contains(Key("t_to_client_connected_ms"))));
  EXPECT_THAT(metadata, Contains(Key("t_to_terminal_ms")));
}

}  // namespace glic
