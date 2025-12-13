// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/background/glic/glic_controller.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicControllerUiTest : public test::InteractiveGlicTest {
 public:
  GlicControllerUiTest() {
    // TODO(b/453696965): Broken in multi-instance.
    disable_multi_instance_feature_list_.InitAndDisableFeature(
        features::kGlicMultiInstance);
  }
  ~GlicControllerUiTest() override = default;

 protected:
  GlicController& glic_controller() { return glic_controller_; }

  GlicController glic_controller_;
  base::test::ScopedFeatureList disable_multi_instance_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicControllerUiTest, Toggle) {
  Profile* profile =
      glic::GlicProfileManager::GetInstance()->GetProfileForLaunch();
  GlicKeyedService* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  ASSERT_FALSE(glic_keyed_service->IsWindowShowing());

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

// TODO (crbug.com/450563739): Re-enable when the test is fixed on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Show DISABLED_Show
#else
#define MAYBE_Show Show
#endif
IN_PROC_BROWSER_TEST_F(GlicControllerUiTest, MAYBE_Show) {
  Profile* profile =
      glic::GlicProfileManager::GetInstance()->GetProfileForLaunch();
  GlicKeyedService* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  ASSERT_FALSE(glic_keyed_service->IsWindowShowing());

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
