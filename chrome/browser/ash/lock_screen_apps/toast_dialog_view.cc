// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/toast_dialog_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/utility/wm_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace lock_screen_apps {

namespace {

constexpr int kDialogWidthDp = 292;

constexpr int kDialogMessageMarginTopDp = 0;
constexpr int kDialogMessageMarginStartDp = 16;
constexpr int kDialogMessageMarginBottomDp = 18;
constexpr int kDialogMessageMarginEndDp = 12;
constexpr int kDialogMessageLineHeightDp = 20;

constexpr int kDialogTitleMarginTopDp = 14;
constexpr int kDialogTitleMarginStartDp = 16;
constexpr int kDialogTitleMarginBottomDp = 5;
constexpr int kDialogTitleMarginEndDp = 0;

}  // namespace

ToastDialogView::ToastDialogView(const std::u16string& app_name,
                                 base::OnceClosure dismissed_callback) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCloseCallback(std::move(dismissed_callback));
  SetModalType(ui::mojom::ModalType::kNone);
  SetShowCloseButton(true);
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_LOCK_SCREEN_NOTE_APP_TOAST_DIALOG_TITLE, app_name));

  SetArrow(views::BubbleBorder::NONE);
  set_margins(gfx::Insets::TLBR(
      kDialogMessageMarginTopDp, kDialogMessageMarginStartDp,
      kDialogMessageMarginBottomDp, kDialogMessageMarginEndDp));
  set_title_margins(
      gfx::Insets::TLBR(kDialogTitleMarginTopDp, kDialogTitleMarginStartDp,
                        kDialogTitleMarginBottomDp, kDialogTitleMarginEndDp));
  set_shadow(views::BubbleBorder::STANDARD_SHADOW);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* label = new views::Label(l10n_util::GetStringFUTF16(
      IDS_LOCK_SCREEN_NOTE_APP_TOAST_DIALOG_MESSAGE, app_name));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SkColorSetARGB(138, 0, 0, 0));
  label->SetLineHeight(kDialogMessageLineHeightDp);
  label->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
  label->SetPreferredSize(
      gfx::Size(kDialogWidthDp, label->GetHeightForWidth(kDialogWidthDp)));
  label->SizeToPreferredSize();

  AddChildView(label);
}

ToastDialogView::~ToastDialogView() = default;

void ToastDialogView::AddedToWidget() {
  std::unique_ptr<views::Label> title =
      views::BubbleFrameView::CreateDefaultTitleLabel(GetWindowTitle());
  title->SetFontList(views::Label::GetDefaultFontList().Derive(
      3, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  GetBubbleFrameView()->SetTitleView(std::move(title));
}

void ToastDialogView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  ash_util::SetupWidgetInitParamsForContainer(
      params, ash::kShellWindowId_SettingBubbleContainer);
}

BEGIN_METADATA(ToastDialogView)
END_METADATA

}  // namespace lock_screen_apps
