// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_state.h"

#include <memory>
#include <optional>

#include "ash/public/cpp/tablet_mode.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/experiences/arc/mojom/input_method_manager.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"

namespace arc {

namespace {

using ::ash::input_method::InputMethodDescriptor;
using ::ash::input_method::InputMethodDescriptors;

mojom::ImeInfoPtr GenerateImeInfo(const std::string& id,
                                  bool enabled,
                                  bool always_allowed) {
  mojom::ImeInfoPtr info = mojom::ImeInfo::New();
  info->ime_id = id;
  info->enabled = enabled;
  info->is_allowed_in_clamshell_mode = always_allowed;
  return info;
}

class FakeDelegate : public ArcInputMethodState::Delegate {
 public:
  bool ShouldArcIMEAllowed() const override { return allowed; }
  InputMethodDescriptor BuildInputMethodDescriptor(
      const mojom::ImeInfoPtr& info) const override {
    return InputMethodDescriptor(info->ime_id, "", "", {}, {}, false,
                                 GURL(info->settings_url), GURL(),
                                 /*handwriting_language=*/std::nullopt);
  }
  bool allowed = false;
};

}  // namespace

TEST(ArcInputMethodState, Constructor) {
  FakeDelegate delegate;

  ArcInputMethodState empty_state(&delegate);
  InputMethodDescriptors empty_vector;
  EXPECT_EQ(0u, empty_state.GetAvailableInputMethods().size());
  EXPECT_EQ(0u, empty_state.GetEnabledInputMethods().size());
}

TEST(ArcInputMethodState, InstallInputMethod) {
  FakeDelegate delegate;

  ArcInputMethodState state(&delegate);
  std::vector<mojom::ImeInfoPtr> imes;
  imes.push_back(GenerateImeInfo("ime_a", true, false));
  imes.push_back(GenerateImeInfo("ime_b", true, true));
  imes.push_back(GenerateImeInfo("ime_c", false, false));
  imes.push_back(GenerateImeInfo("ime_d", false, true));
  state.InitializeWithImeInfo("ime_id", imes);

  InputMethodDescriptors active_imes = state.GetAvailableInputMethods();
  EXPECT_EQ(2u, active_imes.size());
  EXPECT_EQ("ime_b", active_imes[0].id());
  EXPECT_EQ("ime_d", active_imes[1].id());

  InputMethodDescriptors enabled_imes = state.GetEnabledInputMethods();
  EXPECT_EQ(1u, enabled_imes.size());
  EXPECT_EQ("ime_b", enabled_imes[0].id());
}

TEST(ArcInputMethodState, DisableInputMethod) {
  FakeDelegate delegate;

  ArcInputMethodState state(&delegate);
  std::vector<mojom::ImeInfoPtr> imes;
  imes.push_back(GenerateImeInfo("ime_a", true, true));
  state.InitializeWithImeInfo("ime_id", imes);
  EXPECT_EQ(1u, state.GetEnabledInputMethods().size());

  state.DisableInputMethod("ime_a");
  EXPECT_EQ(0u, state.GetEnabledInputMethods().size());
}

TEST(ArcInputMethodState, AllowDisallowInputMethods) {
  FakeDelegate delegate;

  ArcInputMethodState state(&delegate);
  std::vector<mojom::ImeInfoPtr> imes;
  imes.push_back(GenerateImeInfo("ime_a", true, true));
  imes.push_back(GenerateImeInfo("ime_b", true, false));
  state.InitializeWithImeInfo("ime_id", imes);

  EXPECT_EQ(1u, state.GetAvailableInputMethods().size());
  EXPECT_EQ("ime_a", state.GetAvailableInputMethods()[0].id());

  delegate.allowed = true;
  EXPECT_EQ(2u, state.GetAvailableInputMethods().size());

  delegate.allowed = false;
  EXPECT_EQ(1u, state.GetAvailableInputMethods().size());
  EXPECT_EQ("ime_a", state.GetAvailableInputMethods()[0].id());
}

TEST(ArcInputMethodState, ValidateImeInfo) {
  FakeDelegate delegate;
  delegate.allowed = true;

  ArcInputMethodState state(&delegate);
  std::vector<mojom::ImeInfoPtr> imes;

  // Valid IME
  imes.push_back(GenerateImeInfo("ime_valid", true, true));
  imes.back()->settings_url = "intent://valid_settings";

  // Invalid: empty ID
  imes.push_back(GenerateImeInfo("", true, true));

  // Invalid: comma in ID
  imes.push_back(GenerateImeInfo("ime,invalid", true, true));

  // Invalid: duplicate ID
  imes.push_back(GenerateImeInfo("ime_valid", true, true));

  // Malicious: non-intent settings URL
  imes.push_back(GenerateImeInfo("ime_malicious_url", true, true));
  imes.back()->settings_url = "javascript:alert(1)";

  state.InitializeWithImeInfo("ime_id", imes);

  const InputMethodDescriptors available_imes =
      state.GetAvailableInputMethods();
  // Only "ime_valid" and "ime_malicious_url" should be accepted.
  ASSERT_EQ(2u, available_imes.size());
  EXPECT_EQ("ime_valid", available_imes[0].id());
  EXPECT_EQ(GURL("intent://valid_settings"),
            available_imes[0].options_page_url());

  EXPECT_EQ("ime_malicious_url", available_imes[1].id());
  // settings_url should be cleared for non-intent URLs.
  EXPECT_TRUE(available_imes[1].options_page_url().is_empty());
}

}  // namespace arc
