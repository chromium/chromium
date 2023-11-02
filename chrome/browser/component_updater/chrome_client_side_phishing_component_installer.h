// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CHROME_CLIENT_SIDE_PHISHING_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CHROME_CLIENT_SIDE_PHISHING_COMPONENT_INSTALLER_H_

namespace component_updater {

class ComponentUpdateService;

// Call once during startup to make the component update service aware of
// the Client Side Phishing component.
void RegisterClientSidePhishingComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CHROME_CLIENT_SIDE_PHISHING_COMPONENT_INSTALLER_H_
