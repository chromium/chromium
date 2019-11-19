// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_UPDATE_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_LOCALE_LOCALE_UPDATE_CONTROLLER_IMPL_H_

#include <string>

#include "ash/public/cpp/locale_update_controller.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class LocaleChangeObserver {
 public:
  virtual ~LocaleChangeObserver() = default;

  // Called when locale is changed.
  virtual void OnLocaleChanged() = 0;
};

// Observes and handles locale change events.
class LocaleUpdateControllerImpl : public LocaleUpdateController {
 public:
  LocaleUpdateControllerImpl();
  ~LocaleUpdateControllerImpl() override;

  void AddObserver(LocaleChangeObserver* observer);
  void RemoveObserver(LocaleChangeObserver* observer);

 private:
  // LocaleUpdateController:
  void OnLocaleChanged(const std::string& cur_locale,
                       const std::string& from_locale,
                       const std::string& to_locale,
                       OnLocaleChangedCallback callback) override;

  std::string cur_locale_;
  std::string from_locale_;
  std::string to_locale_;
  base::ObserverList<LocaleChangeObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(LocaleUpdateControllerImpl);
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_UPDATE_CONTROLLER_IMPL_H_
