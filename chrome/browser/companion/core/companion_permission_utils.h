// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_COMPANION_PERMISSION_UTILS_H_
#define CHROME_BROWSER_COMPANION_CORE_COMPANION_PERMISSION_UTILS_H_

class PrefService;

namespace companion {

// Returns true if the user, i.e., the local, current profile, is permitted to
// share the the page URL with the remote Companion server.
bool IsUserPermittedToSharePageURLWithCompanion(PrefService* pref_service);

// Returns true if the user, i.e., the local, current profile, is permitted to
// share the information about the page with the remote Companion server.
bool IsUserPermittedToSharePageContentWithCompanion(PrefService* pref_service);

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_COMPANION_PERMISSION_UTILS_H_
