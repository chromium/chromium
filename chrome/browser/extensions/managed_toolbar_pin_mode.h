// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MANAGED_TOOLBAR_PIN_MODE_H_
#define CHROME_BROWSER_EXTENSIONS_MANAGED_TOOLBAR_PIN_MODE_H_

namespace extensions {

// Policy-based behavior for "Pin extension to toolbar" from the extensions
// menu, default is kDefaultUnpinned.
// * kDefaultUnpinned: Extension starts unpinned, but the user can still pin
//                     it afterwards.
// * kForcePinned: Extension starts pinned to the toolbar, and the user
//                 cannot unpin it.
// TODO(crbug.com/40126725): Add kDefaultPinned state.
enum class ManagedToolbarPinMode {
  kDefaultUnpinned = 0,
  kForcePinned,
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MANAGED_TOOLBAR_PIN_MODE_H_
