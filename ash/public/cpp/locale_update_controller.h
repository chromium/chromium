// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOCALE_UPDATE_CONTROLLER_H_
#define ASH_PUBLIC_CPP_LOCALE_UPDATE_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"

namespace ash {

// The locale info to show in the system tray locale detailed view.
struct ASH_PUBLIC_EXPORT LocaleInfo {
  LocaleInfo();
  LocaleInfo(const std::string& iso_code, const std::u16string& display_name);
  LocaleInfo(const LocaleInfo& rhs);
  LocaleInfo(LocaleInfo&& rhs);
  ~LocaleInfo();

  // This ISO code of the locale.
  std::string iso_code;

  // The display name of the locale.
  std::u16string display_name;
};

// Sent as the response to LocaleUpdateController.OnLocaleChanged().
enum class LocaleNotificationResult {
  kAccept,
  kRevert,
};

class LocaleChangeObserver {
 public:
  virtual ~LocaleChangeObserver() = default;

  // Called when locale is changed.
  virtual void OnLocaleChanged() = 0;
};

// Used by Chrome to notify locale change events.
class ASH_PUBLIC_EXPORT LocaleUpdateController {
 public:
  using LocaleChangeConfirmationCallback =
      base::OnceCallback<void(LocaleNotificationResult)>;

  // Get the singleton instance of LocaleUpdateController.
  static LocaleUpdateController* Get();

  // Called when system locale changes, and the user should be notified of the
  // locale change. Used during OOBE, or on user login if the user's preferred
  // locale does not match the login screen locale.
  virtual void OnLocaleChanged() = 0;

  // Called when the locale changes in user session (for example by sync). This
  // will show a notification asking the user to confirm the locale change.
  // |callback| will be called when the user accepts or rejects the locale
  // change.
  virtual void ConfirmLocaleChange(
      const std::string& current_locale,
      const std::string& from_locale,
      const std::string& to_locale,
      LocaleChangeConfirmationCallback callback) = 0;

  virtual void AddObserver(LocaleChangeObserver* observer) = 0;
  virtual void RemoveObserver(LocaleChangeObserver* observer) = 0;

 protected:
  LocaleUpdateController();
  virtual ~LocaleUpdateController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOCALE_UPDATE_CONTROLLER_H_
