// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_LOCALE_MODEL_H_
#define ASH_SYSTEM_MODEL_LOCALE_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/locale_update_controller.h"
#include "base/observer_list.h"

namespace ash {

// Model to store system locale list.
class LocaleModel {
 public:
  class Observer {
   public:
    virtual ~Observer();

    // Notify the observer that the locale list is set.
    virtual void OnLocaleListSet() = 0;
  };

  LocaleModel();

  LocaleModel(const LocaleModel&) = delete;
  LocaleModel& operator=(const LocaleModel&) = delete;

  ~LocaleModel();

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  void SetLocaleList(std::vector<LocaleInfo> locale_list,
                     const std::string& current_locale_iso_code);

  bool ShouldShowCurrentLocaleInStatusArea() const;

  const std::vector<LocaleInfo>& locale_list() { return locale_list_; }

  std::string current_locale_iso_code() const {
    return current_locale_iso_code_;
  }

 private:
  // Adds a fake locale with a label like "German - Deutsch".
  void AddFakeLocale(const std::string& locale);

  std::vector<LocaleInfo> locale_list_;

  std::string current_locale_iso_code_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_LOCALE_MODEL_H_
