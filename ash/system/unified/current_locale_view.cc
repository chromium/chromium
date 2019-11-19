// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/current_locale_view.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace ash {

CurrentLocaleView::CurrentLocaleView(Shelf* shelf) : TrayItemView(shelf) {
  SetVisible(false);
  CreateLabel();
  SetupLabelForTray(label());

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
  label()->SetEnabledColor(
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));

  const std::vector<LocaleInfo>& locales = locale_model->locale_list();
  for (auto& entry : locales) {
    if (entry.iso_code == locale_model->current_locale_iso_code()) {
      const base::string16 description = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_INDICATOR_LOCALE_TOOLTIP, entry.display_name);
      label()->SetTooltipText(description);
      label()->SetCustomAccessibleName(description);
      break;
    }
  }
  Layout();
}

const char* CurrentLocaleView::GetClassName() const {
  return "CurrentLocaleView";
}

}  // namespace ash
