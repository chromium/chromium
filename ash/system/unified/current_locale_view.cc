// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/current_locale_view.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace ash {

CurrentLocaleView::CurrentLocaleView(Shelf* shelf) : TrayItemView(shelf) {
  SetVisible(false);
  CreateLabel();
  SetupLabelForTray(label());
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kUnifiedTrayTextTopPadding, 0, 0, kUnifiedTrayTextRightPadding)));

  Shell::Get()->system_tray_model()->locale()->AddObserver(this);
}

CurrentLocaleView::~CurrentLocaleView() {
  Shell::Get()->system_tray_model()->locale()->RemoveObserver(this);
}

void CurrentLocaleView::OnLocaleListSet() {
  LocaleModel* const locale_model = Shell::Get()->system_tray_model()->locale();
  SetVisible(locale_model->ShouldShowCurrentLocaleInStatusArea());
  label()->SetText(base::i18n::ToUpper(base::UTF8ToUTF16(
      l10n_util::GetLanguage(locale_model->current_locale_iso_code()))));
  UpdateLabelOrImageViewColor(is_active());

  const std::vector<LocaleInfo>& locales = locale_model->locale_list();
  for (auto& entry : locales) {
    if (entry.iso_code == locale_model->current_locale_iso_code()) {
      const std::u16string description = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_INDICATOR_LOCALE_TOOLTIP, entry.display_name);
      label()->SetTooltipText(description);
      label()->SetCustomAccessibleName(description);
      break;
    }
  }
  DeprecatedLayoutImmediately();
}

void CurrentLocaleView::HandleLocaleChange() {
  // Nothing to do here, when this view is used, the locale will be updated
  // using locale_model.
}

void CurrentLocaleView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);

  label()->SetEnabledColorId(active
                                 ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                                 : cros_tokens::kCrosSysOnSurface);
}

BEGIN_METADATA(CurrentLocaleView)
END_METADATA

}  // namespace ash
