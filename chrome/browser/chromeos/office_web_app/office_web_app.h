// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OFFICE_WEB_APP_OFFICE_WEB_APP_H_
#define CHROME_BROWSER_CHROMEOS_OFFICE_WEB_APP_OFFICE_WEB_APP_H_

#include "base/functional/callback.h"

class Profile;

namespace webapps {
enum class InstallResultCode;
}

namespace chromeos {

// The URL to install the Microsoft365 app.
extern const char kMicrosoft365WebAppUrl[];

void InstallMicrosoft365(
    Profile* profile,
    base::OnceCallback<void(webapps::InstallResultCode)> callback);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OFFICE_WEB_APP_OFFICE_WEB_APP_H_
