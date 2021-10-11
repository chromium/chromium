// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_dialog_controller.h"

#include "base/bind.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {

DesksTemplatesDialogController* g_instance = nullptr;

// Helpers to get various strings based on the dialog type.
// TODO(sammiequon): These all need to be localized.
std::u16string GetTitleForType(
    DesksTemplatesDialogController::DialogType type) {
  switch (type) {
    case DesksTemplatesDialogController::DialogType::kUnsupported:
      return u"Unsupported apps in template";
    case DesksTemplatesDialogController::DialogType::kReplace:
      return u"Replace template?";
    case DesksTemplatesDialogController::DialogType::kDelete:
      return u"Delete template?";
  };
}

std::u16string GetAcceptButtonTextForType(
    DesksTemplatesDialogController::DialogType type) {
  switch (type) {
    case DesksTemplatesDialogController::DialogType::kUnsupported:
      return u"Save anyway";
    case DesksTemplatesDialogController::DialogType::kReplace:
      return u"Replace";
    case DesksTemplatesDialogController::DialogType::kDelete:
      return u"Delete";
  };
}

std::u16string GetDescriptionTextForType(
    DesksTemplatesDialogController::DialogType type) {
  switch (type) {
    case DesksTemplatesDialogController::DialogType::kUnsupported:
      return u"Linux apps aren't currently supported. Other apps will be "
             u"saved.";
    case DesksTemplatesDialogController::DialogType::kReplace:
      return u"Template named placeholder already exists";
    case DesksTemplatesDialogController::DialogType::kDelete:
      return u"Placeholder will be permanently deleted";
  };
}

}  // namespace

// The client view of the dialog. Contains a label which is a description, and
// optionally a couple images of unsupported apps. This dialog will block the
// entire system.
class DesksTemplatesDialog : public views::DialogDelegateView {
 public:
  METADATA_HEADER(DesksTemplatesDialog);

  explicit DesksTemplatesDialog(
      DesksTemplatesDialogController::DialogType type) {
    SetModalType(ui::MODAL_TYPE_SYSTEM);
    SetTitle(GetTitleForType(type));
    SetShowCloseButton(false);
    SetButtonLabel(ui::DIALOG_BUTTON_OK, GetAcceptButtonTextForType(type));
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(IDS_APP_CANCEL));

    auto* layout_provider = views::LayoutProvider::Get();
    set_fixed_width(layout_provider->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
        layout_provider->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    AddChildView(views::Builder<views::Label>()
                     .SetText(GetDescriptionTextForType(type))
                     .SetMultiLine(true)
                     .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                     .Build());
  }
  DesksTemplatesDialog(const DesksTemplatesDialog&) = delete;
  DesksTemplatesDialog& operator=(const DesksTemplatesDialog&) = delete;
  ~DesksTemplatesDialog() override = default;
};

BEGIN_METADATA(DesksTemplatesDialog, views::DialogDelegateView)
END_METADATA

//-----------------------------------------------------------------------------
// DesksTemplatesDialogController:

DesksTemplatesDialogController::DesksTemplatesDialogController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

DesksTemplatesDialogController::~DesksTemplatesDialogController() {
  if (dialog_widget_)
    dialog_widget_->CloseNow();

  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
DesksTemplatesDialogController* DesksTemplatesDialogController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void DesksTemplatesDialogController::Show(DialogType type,
                                          aura::Window* root_window) {
  if (dialog_widget_)
    dialog_widget_->CloseNow();

  // The dialog will show on the display associated with `root_window`, and will
  // block all input since it is system modal.
  DCHECK(root_window->IsRootWindow());
  dialog_widget_ = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<DesksTemplatesDialog>(type),
      /*context=*/root_window, /*parent=*/nullptr);
  dialog_widget_->Show();
  dialog_widget_observation_.Observe(dialog_widget_);
}

void DesksTemplatesDialogController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(dialog_widget_, widget);
  dialog_widget_observation_.Reset();
  dialog_widget_ = nullptr;
}

}  // namespace ash
