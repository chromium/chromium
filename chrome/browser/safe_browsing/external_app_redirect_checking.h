// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTERNAL_APP_REDIRECT_CHECKING_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTERNAL_APP_REDIRECT_CHECKING_H_

#include <string>

#include "base/functional/bind.h"

namespace content {
class WebContents;
}

class PrefService;

namespace safe_browsing {

class ClientSafeBrowsingReportRequest;
class SafeBrowsingDatabaseManager;

// Asynchronously check whether we should report to Safe Browsing a redirect by
// `web_contents` to `app_name`. Returns the result asynchronously through
// `callback`.
void ShouldReportExternalAppRedirect(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    content::WebContents* web_contents,
    std::string_view app_name,
    std::string_view uri,
    base::OnceCallback<void(bool)> callback);

// Log the current time as the most recent usage of a redirect to the given app.
void LogExternalAppRedirectTimestamp(PrefService& prefs,
                                     std::string_view app_name);

// Clean up expired redirect timestamps.
void CleanupExternalAppRedirectTimestamps(PrefService& prefs);

// Create the report that will be sent to Safe Browsing, if appropriate.
std::unique_ptr<ClientSafeBrowsingReportRequest> MakeExternalAppRedirectReport(
    content::WebContents* web_contents,
    std::string_view uri);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTERNAL_APP_REDIRECT_CHECKING_H_
