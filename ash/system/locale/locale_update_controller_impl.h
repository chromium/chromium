// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_UPDATE_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_LOCALE_LOCALE_UPDATE_CONTROLLER_IMPL_H_

#include <string>

#include "ash/public/cpp/locale_update_controller.h"
#include "base/observer_list.h"

namespace ash {

// Observes and handles locale change events.
class LocaleUpdateControllerImpl : public LocaleUpdateController {
 public:
  LocaleUpdateControllerImpl();

  LocaleUpdateControllerImpl(const LocaleUpdateControllerImpl&) = delete;
  LocaleUpdateControllerImpl& operator=(const LocaleUpdateControllerImpl&) =
      delete;

  ~LocaleUpdateControllerImpl() override;

  // LocaleUpdateController:
  void AddObserver(LocaleChangeObserver* observer) override;
  void RemoveObserver(LocaleChangeObserver* observer) override;

 private:
  // LocaleUpdateController:
  void OnLocaleChanged() override;
  void ConfirmLocaleChange(const std::string& current_locale,
                           const std::string& from_locale,
                           const std::string& to_locale,
                           LocaleChangeConfirmationCallback callback) override;

  base::ObserverList<LocaleChangeObserver>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_UPDATE_CONTROLLER_IMPL_H_
