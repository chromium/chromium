// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/signin_dialog.h"

#include <memory>

#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash::printing::oauth2 {
namespace {

class SigninDialogTest : public ChromeAshTestBase {
 public:
  SigninDialogTest()
      : ChromeAshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {}
};

}  // namespace

TEST_F(SigninDialogTest, GetDialogPositionUsesWidgetCoordinates) {
  content::RenderViewHostTestEnabler rvh_test_enabler;
  TestingProfile profile;

  auto* dialog = new SigninDialog(&profile);
  dialog->SetTitle(u"https://example.com/");
  views::Widget* widget =
      views::DialogDelegate::CreateDialogWidget(dialog, GetContext(), {});
  widget->Show();

  const gfx::Rect bounds =
      dialog->ConvertRectToWidget(dialog->GetLocalBounds());
  ASSERT_GT(bounds.y(), 0);

  web_modal::WebContentsModalDialogHost* modal_host = dialog;
  constexpr gfx::Size kDialogSize{200, 100};
  EXPECT_EQ(
      gfx::Point(bounds.x() + (bounds.width() - kDialogSize.width()) / 2,
                 bounds.y() + (bounds.height() - kDialogSize.height()) / 2),
      modal_host->GetDialogPosition(kDialogSize));
  EXPECT_EQ(bounds.size(), modal_host->GetMaximumDialogSize());

  widget->CloseNow();
}

}  // namespace ash::printing::oauth2
