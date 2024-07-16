// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_mediator.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"
#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/ime_bridge.h"

namespace ash::input_method {
namespace {

class EditorMediatorTest : public ChromeAshTestBase {
 public:
  EditorMediatorTest() = default;
  ~EditorMediatorTest() override = default;

  TestingProfile& profile() { return profile_; }

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kOrcaDogfood};
  TestingProfile profile_;
};

TEST_F(EditorMediatorTest,
       SurroundingTextChangedDoesNotChangeSelectedTextLength) {
  std::unique_ptr<EditorGeolocationProvider> geolocation_provider =
      std::make_unique<EditorGeolocationMockProvider>("us");
  EditorMediator mediator(&profile(), std::move(geolocation_provider));

  IMEBridge::Get()->SetCurrentInputContext(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  mediator.FetchAndUpdateInputContextForTesting();

  mediator.OnSurroundingTextChanged(u"a", gfx::Range(0, 1));

  EXPECT_EQ(mediator.GetEditorOpportunityMode(), EditorOpportunityMode::kWrite);
}

TEST_F(EditorMediatorTest, CacheContextChangesSelectedTextLength) {
  std::unique_ptr<EditorGeolocationProvider> geolocation_provider =
      std::make_unique<EditorGeolocationMockProvider>("us");
  EditorMediator mediator(&profile(), std::move(geolocation_provider));

  IMEBridge::Get()->SetCurrentInputContext(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  mediator.FetchAndUpdateInputContextForTesting();
  mediator.OnSurroundingTextChanged(u"a", gfx::Range(0, 1));

  mediator.CacheContext();

  EXPECT_EQ(mediator.GetEditorOpportunityMode(),
            EditorOpportunityMode::kRewrite);
}

}  // namespace
}  // namespace ash::input_method
