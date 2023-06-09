// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_URL_HANDLER_H_
#define CHROME_BROWSER_ASH_URL_HANDLER_H_

#include "ui/base/window_open_disposition.h"

class GURL;

namespace ash {

// Tries to open the URL in a Lacros-compatible manner.
// Details are subtle and in flux. Very roughly speaking:
// - External URLs (e.g. http://), are opened in Lacros.
// - Internal URLs (e.g. chrome://) are opened in the appropriate SWA. If the
//   URL does not belong to a SWA, but is allow-listed, it is opened with a
//   generic wrapper SWA to render in an (Ash) app window.
// Returns false iff the URL wasn't opened, for example because Lacros is
// disabled or the URL is a non-SWA chrome:// URL that is not allow-listed.
bool TryOpenUrl(const GURL& url, WindowOpenDisposition disposition);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_URL_HANDLER_H_
