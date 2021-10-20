// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_dialog_controller.h"

#include "ash/strings/grit/ash_strings.h"
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

std::u16string GetStringWithQuotes(const std::u16string& str) {
  return u"\"" + str + u"\"";
}

}  // namespace

// The client view of the dialog. Contains a label which is a description, and
// optionally a couple images of unsupported apps. This dialog will block the
// entire system.
class DesksTemplatesDialog : public views::DialogDelegateView {
 public:
  METADATA_HEADER(DesksTemplatesDialog);

  DesksTemplatesDialog() {
    SetModalType(ui::MODAL_TYPE_SYSTEM);
    SetShowCloseButton(false);
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

    // Add the description for the dialog.
    AddChildView(views::Builder<views::Label>()
                     .CopyAddressTo(&description_label_)
                     .SetMultiLine(true)
                     .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                     .Build());
  }
  DesksTemplatesDialog(const DesksTemplatesDialog&) = delete;
  DesksTemplatesDialog& operator=(const DesksTemplatesDialog&) = delete;
  ~DesksTemplatesDialog() override = default;

  void SetTitleText(int message_id) {
    SetTitle(l10n_util::GetStringUTF16(message_id));
  }

  void SetConfirmButtonText(int message_id) {
    SetButtonLabel(ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(message_id));
  }

  void SetDescriptionText(const std::u16string& text) {
    DCHECK(description_label_);
    description_label_->SetText(text);
  }

 private:
  views::Label* description_label_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   DesksTemplatesDialog,
                   views::DialogDelegateView)
VIEW_BUILDER_PROPERTY(int, TitleText)
VIEW_BUILDER_PROPERTY(int, ConfirmButtonText)
VIEW_BUILDER_PROPERTY(std::u16string, DescriptionText)
END_VIEW_BUILDER

BEGIN_METADATA(DesksTemplatesDialog, views::DialogDelegateView)
END_METADATA

}  // namespace ash

// Must be in global namespace and defined before usage.
DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesDialog)

namespace ash {

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

void DesksTemplatesDialogController::ShowUnsupportedAppsDialog(
    aura::Window* root_window) {
  // TODO(crbug.com/1261623): Add a list of unsupported apps icons.
  auto dialog =
      views::Builder<DesksTemplatesDialog>()
          .SetTitleText(IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_TITLE)
          .SetConfirmButtonText(
              IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_CONFIRM_BUTTON)
          .SetDescriptionText(l10n_util::GetStringUTF16(
              IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_DESCRIPTION))
          .AddChildren(
              views::Builder<views::Label>()
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_APPS_DIALOG_HEADER)))
          .Build();
  CreateDialogWidget(std::move(dialog), root_window);
}

void DesksTemplatesDialogController::ShowReplaceDialog(
    aura::Window* root_window,
    const std::u16string& template_name) {
  auto dialog = views::Builder<DesksTemplatesDialog>()
                    .SetTitleText(IDS_ASH_DESKS_TEMPLATES_REPLACE_DIALOG_TITLE)
                    .SetConfirmButtonText(
                        IDS_ASH_DESKS_TEMPLATES_REPLACE_DIALOG_CONFIRM_BUTTON)
                    .SetDescriptionText(l10n_util::GetStringFUTF16(
                        IDS_ASH_DESKS_TEMPLATES_REPLACE_DIALOG_DESCRIPTION,
                        GetStringWithQuotes(template_name)))
                    .Build();
  CreateDialogWidget(std::move(dialog), root_window);
}

void DesksTemplatesDialogController::ShowDeleteDialog(
    aura::Window* root_window,
    const std::u16string& template_name) {
  auto dialog = views::Builder<DesksTemplatesDialog>()
                    .SetTitleText(IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_TITLE)
                    .SetConfirmButtonText(
                        IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_CONFIRM_BUTTON)
                    .SetDescriptionText(l10n_util::GetStringFUTF16(
                        IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_DESCRIPTION,
                        GetStringWithQuotes(template_name)))
                    .Build();
  CreateDialogWidget(std::move(dialog), root_window);
}

void DesksTemplatesDialogController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(dialog_widget_, widget);
  dialog_widget_observation_.Reset();
  dialog_widget_ = nullptr;
}

void DesksTemplatesDialogController::CreateDialogWidget(
    std::unique_ptr<DesksTemplatesDialog> dialog,
    aura::Window* root_window) {
  if (dialog_widget_)
    dialog_widget_->CloseNow();

  // The dialog will show on the display associated with `root_window`, and will
  // block all input since it is system modal.
  DCHECK(root_window->IsRootWindow());
  dialog_widget_ = views::DialogDelegate::CreateDialogWidget(
      std::move(dialog),
      /*context=*/root_window, /*parent=*/nullptr);
  dialog_widget_->Show();
  dialog_widget_observation_.Observe(dialog_widget_);
}

}  // namespace ash
