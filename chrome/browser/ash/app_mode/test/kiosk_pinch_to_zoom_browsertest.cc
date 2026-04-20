// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/input/switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

using kiosk::test::WaitKioskLaunched;

class PinchEventObserver
    : public content::RenderWidgetHost::InputEventObserver {
 public:
  explicit PinchEventObserver(content::RenderWidgetHost* host) {
    observation_.Observe(host);
  }
  ~PinchEventObserver() override = default;

  void OnInputEvent(const content::RenderWidgetHost& host,
                    const blink::WebInputEvent& event,
                    InputEventSource source) override {
    if (event.GetType() == blink::WebInputEvent::Type::kGesturePinchBegin) {
      pinch_begin_seen_ = true;
    }
  }

  bool pinch_begin_seen() const { return pinch_begin_seen_; }

 private:
  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHost::InputEventObserver>
      observation_{this};
  bool pinch_begin_seen_ = false;
};

}  // namespace

class KiosksPinchToZoomTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  KiosksPinchToZoomTest() {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    if (!IsPinchToZoomSwitchAllowed()) {
      command_line->AppendSwitch(input::switches::kDisablePinch);
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    // We need to set the policy before the Kiosk session starts, so it is
    // correctly applied during launch.
    SetPinchToZoomPolicy(IsPinchToZoomPolicyAllowed());
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetPinchToZoomPolicy(bool allowed) {
    policy::PolicyMap values;
    values.Set(policy::key::kKioskPinchToZoomAllowed,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(allowed), nullptr);
    policy_provider_.UpdateChromePolicy(values);
  }

  bool IsPinchToZoomSwitchAllowed() const { return std::get<0>(GetParam()); }
  bool IsPinchToZoomPolicyAllowed() const { return std::get<1>(GetParam()); }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  KioskMixin kiosk_{
      &mixin_host_,
      KioskMixin::Config{/*name=*/"WebApp",
                         /*auto_launch_account_id=*/
                         KioskMixin::AutoLaunchAccount{
                             KioskMixin::SimpleWebAppOption().account_id},
                         {KioskMixin::SimpleWebAppOption()}}};
};

IN_PROC_BROWSER_TEST_P(KiosksPinchToZoomTest, PinchToZoom) {
  ASSERT_TRUE(WaitKioskLaunched());

  BrowserWindowInterface* browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  content::WebContents* web_contents =
      browser->GetActiveTabInterface()->GetContents();

  bool expected_allowed =
      IsPinchToZoomSwitchAllowed() && IsPinchToZoomPolicyAllowed();

  content::RenderWidgetHost* rwh =
      web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
  PinchEventObserver observer(rwh);

  ui::GestureEventDetails pinch_details(ui::EventType::kGesturePinchBegin);
  pinch_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent pinch_event(10, 10, 0, base::TimeTicks::Now(),
                               pinch_details);
  web_contents->GetRenderWidgetHostView()
      ->GetNativeView()
      ->delegate()
      ->OnGestureEvent(&pinch_event);

  EXPECT_EQ(expected_allowed, observer.pinch_begin_seen());
}

INSTANTIATE_TEST_SUITE_P(All,
                         KiosksPinchToZoomTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace ash
