// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/locale_model.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

LocaleModel::Observer::~Observer() = default;

LocaleModel::LocaleModel() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kQsShowLocaleTile)) {
    AddFakeLocale("de");
    AddFakeLocale("en-US");
    AddFakeLocale("es");
    current_locale_iso_code_ = "en-US";
  }
}

LocaleModel::~LocaleModel() = default;

void LocaleModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LocaleModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void LocaleModel::SetLocaleList(std::vector<LocaleInfo> locale_list,
                                const std::string& current_locale_iso_code) {
  locale_list_ = std::move(locale_list);
  current_locale_iso_code_ = current_locale_iso_code;
  for (auto& observer : observers_)
    observer.OnLocaleListSet();
}

bool LocaleModel::ShouldShowCurrentLocaleInStatusArea() const {
  return !current_locale_iso_code_.empty();
}

void LocaleModel::AddFakeLocale(const std::string& locale) {
  LocaleInfo locale_info;
  locale_info.iso_code = locale;
  locale_info.display_name = l10n_util::GetDisplayNameForLocale(
      locale, /*display_locale=*/"en-US", /*is_for_ui=*/true);
  std::u16string native_display_name =
      l10n_util::GetDisplayNameForLocale(locale, locale,
                                         /*is_for_ui=*/true);
  if (locale_info.display_name != native_display_name) {
    locale_info.display_name += u" - " + native_display_name;
  }
  locale_list_.push_back(locale_info);
}

}  // namespace ash
