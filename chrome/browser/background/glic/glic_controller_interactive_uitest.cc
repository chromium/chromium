// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/background/glic/glic_controller.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicControllerUiTest : public test::InteractiveGlicTest {
 public:
  GlicControllerUiTest() = default;
  ~GlicControllerUiTest() override = default;

 protected:
  GlicController& glic_controller() { return glic_controller_; }

  GlicController glic_controller_;
};

IN_PROC_BROWSER_TEST_F(GlicControllerUiTest, Toggle) {
  Profile* profile =
      glic::GlicProfileManager::GetInstance()->GetProfileForLaunch();
  GlicKeyedService* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  ASSERT_FALSE(glic_keyed_service->window_controller().IsShowing());

  RunTestSequence(
      ObserveState(test::internal::kGlicWindowControllerState,
                   std::ref(window_controller())),
      Do([this]() {
        glic_controller().Toggle(mojom::InvocationSource::kOsButton);
      }),
      WaitForState(test::internal::kGlicWindowControllerState,
                   GlicWindowController::State::kOpen),
      Do([this]() {
        glic_controller().Toggle(mojom::InvocationSource::kOsButton);
      }),
      WaitForState(test::internal::kGlicWindowControllerState,
                   GlicWindowController::State::kClosed));
}

IN_PROC_BROWSER_TEST_F(GlicControllerUiTest, Show) {
  Profile* profile =
      glic::GlicProfileManager::GetInstance()->GetProfileForLaunch();
  GlicKeyedService* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  ASSERT_FALSE(glic_keyed_service->window_controller().IsShowing());

  RunTestSequence(ObserveState(test::internal::kGlicWindowControllerState,
                               std::ref(window_controller())),
                  Do([this]() {
                    glic_controller().Show(mojom::InvocationSource::kOsButton);
                  }),
                  WaitForState(test::internal::kGlicWindowControllerState,
                               GlicWindowController::State::kOpen),
                  Do([this]() {
                    glic_controller().Show(mojom::InvocationSource::kOsButton);
                  }),
                  WaitForState(test::internal::kGlicWindowControllerState,
                               GlicWindowController::State::kOpen));
}

}  // namespace glic
