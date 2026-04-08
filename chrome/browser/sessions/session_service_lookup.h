// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_LOOKUP_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_LOOKUP_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace content {
class WebContents;
}

class SessionServiceBase;

// Returns whether or not the argument specified is accepted and
// tracked by AppSessionService instances.
bool IsRelevantToAppSessionService(BrowserWindowInterface::Type type);
bool IsRelevantToAppSessionService(content::WebContents* web_contents);
bool IsRelevantToAppSessionService(BrowserWindowInterface* browser);

// These helpers help choose between AppSessionService and SessionService.
SessionServiceBase* GetAppropriateSessionServiceForProfile(
    BrowserWindowInterface* browser);

SessionServiceBase* GetAppropriateSessionServiceForSessionRestore(
    BrowserWindowInterface* browser);

SessionServiceBase* GetAppropriateSessionServiceIfExisting(
    BrowserWindowInterface* browser);

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_LOOKUP_H_
