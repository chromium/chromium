// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"

#include <string>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Caches the pointer to `WelcomeTourDialog` instance.
WelcomeTourDialog* g_instance = nullptr;

// Appearance.
constexpr gfx::Size kImagePreferredSize(240, 240);

}  // namespace

// WelcomeTourDialog -----------------------------------------------------------

// static
void WelcomeTourDialog::CreateAndShow(base::OnceClosure accept_callback,
                                      base::OnceClosure cancel_callback,
                                      base::OnceClosure close_callback) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_HelpBubbleContainer);
  params.delegate = new WelcomeTourDialog(std::move(accept_callback),
                                          std::move(cancel_callback),
                                          std::move(close_callback));

  auto* widget = new views::Widget;
  widget->Init(std::move(params));

  // `params` does not specify the initial bounds. Therefore, the dialog shows
  // at the center of the display.
  widget->Show();
}

// static
WelcomeTourDialog* WelcomeTourDialog::Get() {
  return g_instance;
}

WelcomeTourDialog::WelcomeTourDialog(base::OnceClosure accept_callback,
                                     base::OnceClosure cancel_callback,
                                     base::OnceClosure close_callback) {
  CHECK(features::IsWelcomeTourEnabled());

  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  const std::u16string product_name = ui::GetChromeOSDeviceName();
  bool is_welcome_tour_v2_enabled = features::IsWelcomeTourV2Enabled();

  views::Builder<SystemDialogDelegateView>(this)
      .SetAcceptButtonText(l10n_util::GetStringUTF16(
          IDS_ASH_WELCOME_TOUR_DIALOG_ACCEPT_BUTTON_TEXT))
      .SetAcceptCallback(std::move(accept_callback))
      .SetCancelButtonText(l10n_util::GetStringUTF16(
          IDS_ASH_WELCOME_TOUR_DIALOG_CANCEL_BUTTON_TEXT))
      .SetCancelCallback(std::move(cancel_callback))
      .SetCloseCallback(std::move(close_callback))
      .SetDescription(l10n_util::GetStringFUTF16(
          is_welcome_tour_v2_enabled
              ? IDS_ASH_WELCOME_TOUR_DIALOG_DESCRIPTION_TEXT_V2
              : IDS_ASH_WELCOME_TOUR_DIALOG_DESCRIPTION_TEXT,
          product_name))
      .SetModalType(ui::mojom::ModalType::kSystem)
      .SetProperty(views::kElementIdentifierKey, kWelcomeTourDialogElementId)
      .SetTitleText(l10n_util::GetStringFUTF16(
          is_welcome_tour_v2_enabled ? IDS_ASH_WELCOME_TOUR_DIALOG_TITLE_TEXT_V2
                                     : IDS_ASH_WELCOME_TOUR_DIALOG_TITLE_TEXT,
          product_name))
      .SetTopContentView(views::Builder<views::ImageView>()
                             .SetImage(ui::ResourceBundle::GetSharedInstance()
                                           .GetThemedLottieImageNamed(
                                               IDR_WELCOME_TOUR_DIALOG_IMAGE))
                             .SetPreferredSize(kImagePreferredSize))
      .BuildChildren();
}

WelcomeTourDialog::~WelcomeTourDialog() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

BEGIN_METADATA(WelcomeTourDialog)
END_METADATA

}  // namespace ash
