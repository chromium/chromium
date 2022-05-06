// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_PREFS_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_PREFS_H_

class PrefRegistrySimple;

namespace ash::personalization_app::prefs {

// Registers profile prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace ash::personalization_app::prefs

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_PREFS_H_
