// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MANAGED_INSTALLATION_MODE_H_
#define CHROME_BROWSER_EXTENSIONS_MANAGED_INSTALLATION_MODE_H_

namespace extensions {

// Policy-based installation mode for extensions, default is kAllowed.
// * kAllowed: Extension can be installed.
// * kBlocked: Extension cannot be installed.
// * kForced: Extension will be installed automatically and cannot be disabled.
// * kRecommended: Extension will be installed automatically but can be
//                 disabled.
// * kRemoved:  Extension cannot be installed and will be automatically removed.
enum class ManagedInstallationMode {
  kAllowed = 0,
  kBlocked,
  kForced,
  kRecommended,
  kRemoved,
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MANAGED_INSTALLATION_MODE_H_
