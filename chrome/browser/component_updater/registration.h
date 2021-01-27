// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_REGISTRATION_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_REGISTRATION_H_

class PrefService;

namespace base {
class FilePath;
}

namespace component_updater {

void RegisterComponentsForUpdate(bool is_off_the_record_profile,
                                 PrefService* profile_prefs,
                                 const base::FilePath& profile_path);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_REGISTRATION_H_
