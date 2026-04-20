// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "base/auto_reset.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom-shared.h"

namespace extensions {

namespace {
// workflow: COM_KIOSK_CUJ4_TASK6_WF1
constexpr char kChromeAppVirtualKeyboardTag[] =
    "screenplay-1194f129-d36c-4a43-adc6-aa8166f7781d";
}  // namespace

class VirtualKeyboardApiTest : public ExtensionApiTest {
 public:
  VirtualKeyboardApiTest() = default;

  VirtualKeyboardApiTest(const VirtualKeyboardApiTest&) = delete;
  VirtualKeyboardApiTest& operator=(const VirtualKeyboardApiTest&) = delete;

  ~VirtualKeyboardApiTest() override = default;

  void SetUp() override {
    feature_session_type_ =
        ScopedCurrentFeatureSessionType(mojom::FeatureSessionType::kKiosk);
    ExtensionApiTest::SetUp();
  }

  void TearDown() override {
    feature_session_type_.reset();
    ExtensionApiTest::TearDown();
  }

 private:
  std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
      feature_session_type_;
};

IN_PROC_BROWSER_TEST_F(VirtualKeyboardApiTest, Test) {
  base::AddFeatureIdTagToTestResult(kChromeAppVirtualKeyboardTag);
  VirtualKeyboardAPI* api =
      BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>::Get(profile());
  ASSERT_TRUE(api);
  ASSERT_TRUE(api->delegate());

  // Start with a keyboard with all features turned off.
  keyboard::KeyboardConfig config = {
      .auto_complete = false,
      .auto_correct = false,
      .auto_capitalize = false,
      .handwriting = false,
      .spell_check = false,
      .voice_input = false,
  };
  ChromeKeyboardControllerClient::Get()->SetKeyboardConfig(config);

  EXPECT_TRUE(RunExtensionTest("virtual_keyboard"));
}

}  // namespace extensions
