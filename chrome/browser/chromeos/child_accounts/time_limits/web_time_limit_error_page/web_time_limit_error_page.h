// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_ERROR_PAGE_WEB_TIME_LIMIT_ERROR_PAGE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_ERROR_PAGE_WEB_TIME_LIMIT_ERROR_PAGE_H_

#include <string>
#include "base/optional.h"

namespace base {

class TimeDelta;

}  // namespace base

// Generates the appropriate time limit error page for Chrome.
// |domain| is the domain of the website that is being paused.
// |title| title is the title of the webcontents before it got paused. If there
// was no title set for the webcontents, the title will be "Chrome got paused".
// |time_limit| is used to specify the amount of time the user can use Chrome
// and PWAs the following day.
// |app_locale| is used to specify the locale used by the browser.
std::string GetWebTimeLimitChromeErrorPage(
    const std::string& domain,
    const base::Optional<std::u16string>& title,
    base::TimeDelta time_limit,
    const std::string& app_locale);

// Generates the appropriate time limit error page for PWAs.
std::string GetWebTimeLimitAppErrorPage(base::TimeDelta time_limit,
                                        const std::string& app_locale,
                                        const std::string& app_name);

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_ERROR_PAGE_WEB_TIME_LIMIT_ERROR_PAGE_H_
