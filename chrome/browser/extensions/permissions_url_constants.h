// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PERMISSIONS_URL_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_PERMISSIONS_URL_CONSTANTS_H_

// Constants that are only needed by the //chrome/browser layer, and thus
// don't need to be in //extensions/permissions.
namespace extension_permissions_constants {

extern const char kRuntimeHostPermissionsHelpURL[];
extern const char kExtensionsSitePermissionsURL[];
extern const char kShowAccessRequestsInToolbarHelpURL[];

}  // namespace extension_permissions_constants

#endif  // CHROME_BROWSER_EXTENSIONS_PERMISSIONS_URL_CONSTANTS_H_
