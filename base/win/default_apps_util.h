// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_DEFAULT_APPS_UTIL_H_
#define BASE_WIN_DEFAULT_APPS_UTIL_H_

#include "base/base_export.h"
#include "base/strings/string_piece.h"

namespace base::win {

// Returns true if the OS has a default apps settings dialog. This method
// returns false on Win7.
BASE_EXPORT bool CanLaunchDefaultAppsSettingsModernDialog();

// Launches the Windows 'settings' modern app with the 'default apps' view
// focused. If `protocol` is not empty, it also highlights the `protocol` in
// the dialog. Returns true if the default apps dialog was successfully opened,
// and the `protocol`, if not empty, was highlighted.
BASE_EXPORT bool LaunchDefaultAppsSettingsModernDialog(
    base::WStringPiece protocol);

}  // namespace base::win

#endif  // BASE_WIN_DEFAULT_APPS_UTIL_H_
