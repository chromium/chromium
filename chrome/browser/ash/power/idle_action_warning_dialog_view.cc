// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/idle_action_warning_dialog_view.h"

#include <algorithm>

#include "base/location.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
const int kCountdownUpdateIntervalMs = 1000;  // 1 second.
}  // namespace

IdleActionWarningDialogView::IdleActionWarningDialogView(
    base::TimeTicks idle_action_time)
    : idle_action_time_(idle_action_time) {
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  SetModalType(ui::mojom::ModalType::kSystem);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText)));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_IDLE_WARNING_LOGOUT_WARNING));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);

  // Shown on the root window for new windows.
  views::DialogDelegate::CreateDialogWidget(this, nullptr /* context */,
                                            nullptr /* parent */)
      ->Show();

  update_timer_.Start(FROM_HERE, base::Milliseconds(kCountdownUpdateIntervalMs),
                      this, &IdleActionWarningDialogView::UpdateTitle);
}

void IdleActionWarningDialogView::CloseDialog() {
  GetWidget()->Close();
}

void IdleActionWarningDialogView::Update(base::TimeTicks idle_action_time) {
  idle_action_time_ = idle_action_time;
  UpdateTitle();
}

std::u16string IdleActionWarningDialogView::GetWindowTitle() const {
  const base::TimeDelta time_until_idle_action =
      std::max(idle_action_time_ - base::TimeTicks::Now(), base::TimeDelta());
  return l10n_util::GetStringFUTF16(
      IDS_IDLE_WARNING_TITLE,
      ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_LONG, 10,
                               time_until_idle_action));
}

IdleActionWarningDialogView::~IdleActionWarningDialogView() = default;

void IdleActionWarningDialogView::UpdateTitle() {
  GetWidget()->UpdateWindowTitle();
}

BEGIN_METADATA(IdleActionWarningDialogView)
END_METADATA

}  // namespace ash
