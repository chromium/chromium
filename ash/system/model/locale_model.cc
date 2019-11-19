// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/locale_model.h"

namespace ash {

LocaleModel::Observer::~Observer() = default;

LocaleModel::LocaleModel() = default;

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

}  // namespace ash
