// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_function_details.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/chrome_extension_test.h"
#include "chrome/browser/extensions/test_standalone_window_controller.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

class TestDispatcherDelegate : public ExtensionFunctionDispatcher::Delegate {
 public:
  explicit TestDispatcherDelegate(WindowController* controller)
      : controller_(controller) {}
  ~TestDispatcherDelegate() override = default;

  WindowController* GetExtensionWindowController() override {
    return controller_;
  }

 private:
  raw_ptr<WindowController> controller_;
};

}  // namespace

using ChromeExtensionFunctionDetailsUnitTest = ChromeExtensionTest;

// Verifies that ChromeExtensionFunctionDetails::GetCurrentWindowController
// returns a standalone WindowController without dereferencing a null
// BrowserWindowInterface.
TEST_F(ChromeExtensionFunctionDetailsUnitTest,
       GetCurrentWindowController_StandaloneWindowController) {
  const SessionID window_id = SessionID::NewUnique();
  TestStandaloneWindowController controller(
      /*base_window=*/nullptr, profile(), window_id, /*web_contents=*/nullptr);
  ASSERT_EQ(nullptr, controller.GetBrowserWindowInterface());

  TestDispatcherDelegate delegate(&controller);
  ExtensionFunctionDispatcher dispatcher(profile());
  dispatcher.set_delegate(&delegate);

  auto function = base::MakeRefCounted<TabsGetFunction>();
  function->ignore_did_respond_for_testing();
  function->SetDispatcher(dispatcher.AsWeakPtr());
  function->SetBrowserContextForTesting(profile());

  ChromeExtensionFunctionDetails details(function.get());
  EXPECT_EQ(&controller, details.GetCurrentWindowController());
}

}  // namespace extensions
