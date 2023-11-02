// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_CONSTANTS_H_

namespace extensions {

// Expected |kExternalInstallDefaultButtonKey| parameter values from Feature
// parameters or from the Webstore response.

// The confirmation button (enable the extension) is the default.
extern const char kDefaultDialogButtonSettingOk[];
// The cancel button (remove the extension) is the default.
extern const char kDefaultDialogButtonSettingCancel[];
// Neither button is the default.
extern const char kDefaultDialogButtonSettingNoDefault[];

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_ERROR_CONSTANTS_H_
