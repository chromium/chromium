// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_GATE_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_GATE_H_

namespace extensions {

class Extension;

// An interface that ExtensionService inquires for whether extension installs
// should be delayed and whether to finish/abort delayed installs.
class InstallGate {
 public:
  // Actions for a pending install.
  enum Action {
    INSTALL,  // Proceed to finish the install.
    DELAY,    // Delay the install.
    ABORT     // Abort the install.
  };

  virtual ~InstallGate() = default;

  // Invoked by ExtensionService to check what to do with a pending install of
  // the given extension. |extension| is an unpacked new extension to be
  // installed. |install_immediately| is the flag associated with the install.
  virtual Action ShouldDelay(const Extension* extension,
                             bool install_immediately) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_GATE_H_
