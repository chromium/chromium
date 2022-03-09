// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_icon_loading_indicator_view.h"

#include "ash/test/ash_test_base.h"
#include "ui/views/controls/image_view.h"

namespace ash {

class EcheIconLoadingIndicatorViewTest : public AshTestBase {
 public:
  EcheIconLoadingIndicatorViewTest() = default;

  EcheIconLoadingIndicatorViewTest(const EcheIconLoadingIndicatorViewTest&) =
      delete;
  EcheIconLoadingIndicatorViewTest& operator=(
      const EcheIconLoadingIndicatorViewTest&) = delete;

  ~EcheIconLoadingIndicatorViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    icon_ = std::make_unique<views::ImageView>();
    eche_icon_loading_indicatior_view_ =
        std::make_unique<EcheIconLoadingIndicatorView>(icon_.get());
  }

  void TearDown() override {
    eche_icon_loading_indicatior_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  EcheIconLoadingIndicatorView* eche_icon_loading_indicatior_view() {
    return eche_icon_loading_indicatior_view_.get();
  }

 private:
  std::unique_ptr<EcheIconLoadingIndicatorView>
      eche_icon_loading_indicatior_view_;
  std::unique_ptr<views::ImageView> icon_;
};

TEST_F(EcheIconLoadingIndicatorViewTest, SetAnimating) {
  // The loading indicator default is visible and not animating.
  EXPECT_TRUE(eche_icon_loading_indicatior_view()->GetVisible());
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetAnimating());

  eche_icon_loading_indicatior_view()->SetVisible(false);
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetVisible());

  // The loading indicator should be invisible and not animating if we set
  // animating to false.
  eche_icon_loading_indicatior_view()->SetAnimating(false);
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetVisible());
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetAnimating());

  // The loading indicator shows up and animates if we set animating to true.
  eche_icon_loading_indicatior_view()->SetAnimating(true);
  EXPECT_TRUE(eche_icon_loading_indicatior_view()->GetVisible());
  EXPECT_TRUE(eche_icon_loading_indicatior_view()->GetAnimating());

  // Again, the loading indicator is invisible and not animating if we set it
  // back.
  eche_icon_loading_indicatior_view()->SetAnimating(false);
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetVisible());
  EXPECT_FALSE(eche_icon_loading_indicatior_view()->GetAnimating());
}

}  // namespace ash