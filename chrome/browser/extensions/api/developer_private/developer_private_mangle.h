// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_MANGLE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_MANGLE_H_

namespace extensions {

namespace api {
namespace developer_private {
struct ItemInfo;
struct ExtensionInfo;
}
}

namespace developer_private_mangle {

// Converts a developer_private::ExtensionInfo into a
// developer_private::ItemInfo for compatability with deprecated API
// functions.
api::developer_private::ItemInfo MangleExtensionInfo(
    const api::developer_private::ExtensionInfo& info);

}  // namespace developer_private_mangle
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_MANGLE_H_
