// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_lookup.h"

#include <memory>
#include <vector>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

using AcceleratorDetails = AcceleratorLookup::AcceleratorDetails;

bool CompareAccelerators(const std::vector<AcceleratorDetails>& expected,
                         const std::vector<AcceleratorDetails>& actual) {
  if (expected.size() != actual.size()) {
    return false;
  }

  for (size_t i = 0; i < expected.size(); ++i) {
    const bool accelerators_equal =
        expected[i].accelerator == actual[i].accelerator;
    const bool key_display_equal =
        expected[i].key_display == actual[i].key_display;
    if (!accelerators_equal || !key_display_equal) {
      return false;
    }
  }

  return true;
}

}  // namespace

class AcceleratorLookupTest : public AshTestBase {
 public:
  AcceleratorLookupTest() = default;
  ~AcceleratorLookupTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kShortcutCustomization);
    AshTestBase::SetUp();
    config_ = Shell::Get()->ash_accelerator_configuration();
    accelerator_lookup_ = Shell::Get()->accelerator_lookup();
  }

  void TearDown() override {
    config_ = nullptr;
    accelerator_lookup_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<AshAcceleratorConfiguration> config_;
  raw_ptr<AcceleratorLookup> accelerator_lookup_;
};

TEST_F(AcceleratorLookupTest, NoAccelerators) {
  config_->Initialize({});

  std::vector<AcceleratorDetails> accelerators =
      accelerator_lookup_->GetAcceleratorsForAction(
          AcceleratorAction::kSwitchToLastUsedIme);

  EXPECT_TRUE(accelerators.empty());
}

TEST_F(AcceleratorLookupTest, LoadAndFetchAccelerator) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  const std::vector<ui::Accelerator> expected_accelerators = {
      {ui::VKEY_A, ui::EF_CONTROL_DOWN},
  };

  std::vector<AcceleratorDetails> actual =
      accelerator_lookup_->GetAcceleratorsForAction(
          AcceleratorAction::kSwitchToLastUsedIme);

  std::vector<AcceleratorDetails> expected = {
      {{ui::VKEY_A, ui::EF_CONTROL_DOWN}, std::u16string(u"a")},
  };

  EXPECT_TRUE(CompareAccelerators(expected, actual));
}

TEST_F(AcceleratorLookupTest, ModifiedAccelerator) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  std::vector<AcceleratorDetails> expected = {
      {{ui::VKEY_SPACE, ui::EF_CONTROL_DOWN}, std::u16string(u"space")},
  };

  std::vector<AcceleratorDetails> actual =
      accelerator_lookup_->GetAcceleratorsForAction(
          AcceleratorAction::kSwitchToLastUsedIme);

  EXPECT_TRUE(CompareAccelerators(expected, actual));

  config_->AddUserAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                              {ui::VKEY_A, ui::EF_COMMAND_DOWN});

  expected = {
      {{ui::VKEY_SPACE, ui::EF_CONTROL_DOWN}, std::u16string(u"space")},
      {{ui::VKEY_A, ui::EF_COMMAND_DOWN}, std::u16string(u"a")},
  };

  actual = accelerator_lookup_->GetAcceleratorsForAction(
      AcceleratorAction::kSwitchToLastUsedIme);
  EXPECT_TRUE(CompareAccelerators(expected, actual));
}

TEST_F(AcceleratorLookupTest, RemovedAccelerator) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);
  config_->RemoveAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                             {ui::VKEY_SPACE, ui::EF_CONTROL_DOWN});

  std::vector<AcceleratorDetails> accelerators =
      accelerator_lookup_->GetAcceleratorsForAction(
          AcceleratorAction::kBrightnessDown);

  EXPECT_TRUE(accelerators.empty());
}

}  // namespace ash
