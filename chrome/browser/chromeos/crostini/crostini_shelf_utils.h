// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHELF_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHELF_UTILS_H_

#include <string>
#include "base/strings/string_piece_forward.h"

class Profile;

namespace crostini {

// The installer/upgrader should set the id on the window so that it will appear
// on the shelf.
extern const char kCrostiniInstallerShelfId[];
extern const char kCrostiniUpgraderShelfId[];

// Returns a shelf app id for an exo window startup id or app id.
//
// First try to return a desktop file id matching the |window_startup_id|.
//
// If the app id is empty, returns empty string. If we can uniquely identify
// a registry entry, returns the crostini app id for that. Otherwise, returns
// the string pointed to by |window_app_id|, prefixed by "crostini:".
//
// As the window app id is derived from fields set by the app itself, it is
// possible for an app to masquerade as a different app.
std::string GetCrostiniShelfAppId(const Profile* profile,
                                  const std::string* window_app_id,
                                  const std::string* window_startup_id);

// Returns whether the app_id is an unmatched Crostini app id.
bool IsUnmatchedCrostiniShelfAppId(base::StringPiece shelf_app_id);

// Returns whether the app_id is a Crostini app id.
bool IsCrostiniShelfAppId(const Profile* profile,
                          base::StringPiece shelf_app_id);

// Returns the title for the specified app.
std::u16string GetCrostiniShelfTitle(base::StringPiece shelf_app_id);

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHELF_UTILS_H_
