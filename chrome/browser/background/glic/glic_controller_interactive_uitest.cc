// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_controller.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicControllerUiTest : public GlicBrowserTest {
 public:
  GlicControllerUiTest() = default;
  ~GlicControllerUiTest() override = default;

 protected:
  GlicKeyedService* glic_service() {
    return GlicKeyedService::Get(GetProfile());
  }
  GlicController& glic_controller() { return glic_controller_; }

  GlicController glic_controller_;
};

IN_PROC_BROWSER_TEST_F(GlicControllerUiTest, Toggle) {
  ASSERT_FALSE(coordinator().IsAnyPanelShowing());

  glic_controller().Toggle(mojom::InvocationSource::kOsButton);
  ASSERT_OK(WaitForGlicOpen());

  glic_controller().Toggle(mojom::InvocationSource::kOsButton);
  ASSERT_OK(WaitForGlicClose());
}

// TODO (crbug.com/450563739): Re-enable when the test is fixed on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Show DISABLED_Show
#else
#define MAYBE_Show Show
#endif
IN_PROC_BROWSER_TEST_F(GlicControllerUiTest, MAYBE_Show) {
  ASSERT_FALSE(coordinator().IsAnyPanelShowing());

  glic_controller().Show(mojom::InvocationSource::kOsButton);
  ASSERT_OK(WaitForGlicOpen());

  glic_controller().Show(mojom::InvocationSource::kOsButton);
  ASSERT_OK(WaitForGlicOpen());
}

}  // namespace glic
