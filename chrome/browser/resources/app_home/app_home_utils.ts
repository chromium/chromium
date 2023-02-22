// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom actions added here also need to be added to
// tools/metrics/actions/actions.xml
export enum AppHomeUserAction {
  APP_HOME_INIT = 'AppHome_NavigationComplete',
  CONTEXT_MENU_TRIGGERED = 'AppHome_CustomContextMenuTriggered',
  OPEN_IN_WINDOW_CHECKED = 'AppHome_OpenInWindowChecked',
  OPEN_IN_WINDOW_UNCHECKED = 'AppHome_OpenInWindowUnchecked',
  LAUNCH_AT_STARTUP_CHECKED = 'AppHome_LaunchAtStartupChecked',
  LAUNCH_AT_STARTUP_UNCHECKED = 'AppHome_LaunchAtStartupUnchecked',
  CREATE_SHORTCUT = 'AppHome_CreateShortcut',
  INSTALL_APP_LOCALLY = 'AppHome_InstallAppLocally',
  UNINSTALL = 'AppHome_Uninstall',
  OPEN_APP_SETTINGS = 'AppHome_OpenAppSettings',
  LAUNCH_WEB_APP = 'AppHome_LaunchApp',
  LAUNCH_DEPRECATED_APP = 'AppHome_LaunchDeprecatedApp',
}

export function recordUserAction(metricName: AppHomeUserAction): void {
  chrome.metricsPrivate.recordUserAction(metricName);
}
