// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OS_URL_HANDLER_H_
#define CHROME_BROWSER_ASH_OS_URL_HANDLER_H_

#include "url/gurl.h"

namespace ash {

// Tries to open the given url using the OS_URL_HANDLER SWA.
//
// We use this when the system is in Lacros-Only mode (i.e. Ash as browser is
// disabled) to open pages that still live in Ash. They are presented in a
// minimal window without any web navigation features like address bar etc.
//
// This fails (returns false) primarily when the URL can't be handled, i.e. when
// it is not allow-listed or when it is already associated with a link-capturing
// SWA.
bool TryLaunchOsUrlHandler(const GURL& url);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OS_URL_HANDLER_H_
