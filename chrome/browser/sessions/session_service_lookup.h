// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_LOOKUP_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_LOOKUP_H_

#include "chrome/browser/ui/browser.h"

class SessionServiceBase;

// Returns whether or not the argument specified is accepted and
// tracked by AppSessionService instances.
bool IsRelevantToAppSessionService(Browser::Type type);
bool IsRelevantToAppSessionService(content::WebContents* web_contents);
bool IsRelevantToAppSessionService(Browser* browser);

// These helpers help choose between AppSessionService and SessionService.
SessionServiceBase* GetAppropriateSessionServiceForProfile(
    const Browser* browser);

SessionServiceBase* GetAppropriateSessionServiceForSessionRestore(
    const Browser* browser);

SessionServiceBase* GetAppropriateSessionServiceIfExisting(
    const Browser* browser);

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_LOOKUP_H_
