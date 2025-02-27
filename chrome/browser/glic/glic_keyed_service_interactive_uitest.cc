// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace glic {

class GlicKeyedServiceUiTest : public test::InteractiveGlicTest {
 public:
  GlicKeyedServiceUiTest() = default;
  ~GlicKeyedServiceUiTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicKeyedServiceUiTest,
                       OpeningProfilePickerClosesPanel) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  Do([&]() { glic_service()->ShowProfilePicker(); }),
                  CheckControllerHasWidget(false));
}

}  // namespace glic
