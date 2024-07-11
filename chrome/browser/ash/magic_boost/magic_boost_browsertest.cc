// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "ash/test/ash_test_util.h"
#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/mahi/public/cpp/mahi_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Runs a specific callback when the observed view is deleted.
class ViewDeletionObserver : public ::views::ViewObserver {
 public:
  ViewDeletionObserver(views::View* view,
                       base::RepeatingClosure on_delete_callback)
      : on_delete_callback_(on_delete_callback) {
    observation_.Observe(view);
  }

 private:
  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    observation_.Reset();
    on_delete_callback_.Run();
  }

  base::RepeatingClosure on_delete_callback_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

// Waits until the content view specified by `widget` is closed.
void WaitUntilViewClosed(views::Widget* widget) {
  ASSERT_TRUE(widget);

  base::RunLoop run_loop;
  ViewDeletionObserver view_observer(
      widget->GetContentsView(),
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace

class MagicBoostBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kMagicBoost,
                              chromeos::features::kOrca,
                              chromeos::features::kFeatureManagementOrca},
        /*disabled_features=*/{});

    InProcessBrowserTest::SetUp();
  }

 protected:
  ui::test::EventGenerator& event_generator() { return *event_generator_; }

 private:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kUseFakeMahiManager);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::GetPrimaryRootWindow());
  }

  base::test::ScopedFeatureList feature_list_;
  base::AutoReset<bool> ignore_mahi_secret_key_ =
      switches::SetIgnoreMahiSecretKeyForTest();
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

IN_PROC_BROWSER_TEST_F(MagicBoostBrowserTest, ShowDisclaimerView) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));

  // Right click on the web content to show the opt in card.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();

  // Finds the opt in card and still cannot find the disclaimer view.
  views::Widget* opt_in_card_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest());
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  ASSERT_TRUE(opt_in_card_widget);

  // Left click on the accept button in the opt in card.
  const views::View* const accept_button =
      opt_in_card_widget->GetContentsView()->GetViewByID(
          chromeos::magic_boost::ViewId::OptInCardPrimaryButton);
  ASSERT_TRUE(accept_button);
  event_generator().MoveMouseTo(
      accept_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  // Closes the opt in card and shows the disclaimer view.
  WaitUntilViewClosed(opt_in_card_widget);
  EXPECT_TRUE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_F(MagicBoostBrowserTest, DeclineOptIn) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));

  // Right click on the web content to show the opt in card.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();

  // Finds the opt in card and still cannot find the disclaimer view.
  views::Widget* opt_in_card_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest());
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  ASSERT_TRUE(opt_in_card_widget);

  // Left click on the decline button in the opt in card.
  const views::View* const decline_button =
      opt_in_card_widget->GetContentsView()->GetViewByID(
          chromeos::magic_boost::ViewId::OptInCardSecondaryButton);
  ASSERT_TRUE(decline_button);
  event_generator().MoveMouseTo(
      decline_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  // Closes the opt in card. Not showing the disclaimer view.
  WaitUntilViewClosed(opt_in_card_widget);
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));

  // TODO(b/349852159): Check the pref value after clicking the decline button
  // with both Orca included or not.
}

}  // namespace ash
