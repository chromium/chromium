// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_FIRST_PARTY_SETS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_FIRST_PARTY_SETS_COMPONENT_INSTALLER_H_

namespace component_updater {

class ComponentUpdateService;

// Call once during startup to make the component update service aware of
// the First-Party Sets component.
void RegisterFirstPartySetsComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_FIRST_PARTY_SETS_COMPONENT_INSTALLER_H_
