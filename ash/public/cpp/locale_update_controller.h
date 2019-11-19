// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOCALE_UPDATE_CONTROLLER_H_
#define ASH_PUBLIC_CPP_LOCALE_UPDATE_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/strings/string16.h"

namespace ash {

// The locale info to show in the system tray locale detailed view.
struct ASH_PUBLIC_EXPORT LocaleInfo {
  LocaleInfo();
  LocaleInfo(const std::string& iso_code, const base::string16& display_name);
  LocaleInfo(const LocaleInfo& rhs);
  LocaleInfo(LocaleInfo&& rhs);
  ~LocaleInfo();

  // This ISO code of the locale.
  std::string iso_code;

  // The display name of the locale.
  base::string16 display_name;
};

// Sent as the response to LocaleUpdateController.OnLocaleChanged().
enum class LocaleNotificationResult {
  kAccept,
  kRevert,
};

// Used by Chrome to notify locale change events.
class ASH_PUBLIC_EXPORT LocaleUpdateController {
 public:
  // Get the singleton instance of LocaleUpdateController.
  static LocaleUpdateController* Get();

  // When this is called in OOBE, it returns kAccept immediately after invoking
  // observer callbacks. |current|, |from|, and |to| are ignored in this case.
  // Otherwise it displays a notification in ash prompting the user whether to
  // accept a change in the locale. If the user clicks the accept button (or
  // closes the notification), OnLocaleChange() returns kAccept. If the user
  // clicks the revert button, returns kRevert.
  using OnLocaleChangedCallback =
      base::OnceCallback<void(LocaleNotificationResult)>;
  virtual void OnLocaleChanged(const std::string& current,
                               const std::string& from,
                               const std::string& to,
                               OnLocaleChangedCallback callback) = 0;

 protected:
  LocaleUpdateController();
  virtual ~LocaleUpdateController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOCALE_UPDATE_CONTROLLER_H_
