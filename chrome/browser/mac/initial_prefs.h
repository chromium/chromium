// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_INITIAL_PREFS_H_
#define CHROME_BROWSER_MAC_INITIAL_PREFS_H_

#include "base/files/file_path.h"

namespace initial_prefs {

// Returns the path to the initial preferences file. Note that this path may be
// empty (in the case where this type of build cannot have an initial
// preferences file) or may not actually exist on the filesystem.
base::FilePath InitialPrefsPath();

}  // namespace initial_prefs

#endif  // CHROME_BROWSER_MAC_INITIAL_PREFS_H_
