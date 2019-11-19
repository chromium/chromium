// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVE_INSTALL_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVE_INSTALL_DATA_H_

#include <string>

namespace extensions {

// Details of an active extension install.
struct ActiveInstallData {
  ActiveInstallData() = default;
  explicit ActiveInstallData(const std::string& extension_id);

  std::string extension_id;
  int percent_downloaded = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVE_INSTALL_DATA_H_
