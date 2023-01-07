// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PNACL_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PNACL_COMPONENT_INSTALLER_H_

namespace pnacl {
// Returns true if PNaCl actually needs an on-demand component update.
// E.g., if PNaCl is not yet installed and the user is loading a PNaCl app,
// or the current version is behind chrome's version, and is ABI incompatible
// with chrome. If not necessary, returns false.
// May conservatively return false before PNaCl is registered, but
// should return the right answer after it is registered.
bool NeedsOnDemandUpdate();
}

namespace component_updater {

class ComponentUpdateService;

void RegisterPnaclComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PNACL_COMPONENT_INSTALLER_H_
