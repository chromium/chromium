// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {InstallReason, InstallSource, RunOnOsLoginMode, WindowMode} from 'chrome://resources/cr_components/app_management/constants.js';

export function createDummyApp(): App {
  return {
    id: '',
    type: AppType.kUnknown,
    title: '',
    description: '',
    version: '',
    size: '',
    installReason: InstallReason.kUnknown,
    permissions: {},
    hideMoreSettings: false,
    hidePinToShelf: false,
    isPreferredApp: false,
    windowMode: WindowMode.kWindow,
    hideWindowMode: false,
    resizeLocked: false,
    hideResizeLocked: true,
    supportedLinks: [],
    runOnOsLogin: {
      loginMode: RunOnOsLoginMode.kNotRun,
      isManaged: false,
    },
    fileHandlingState: null,
    installSource: InstallSource.kUnknown,
    appSize: '',
    dataSize: '',
    publisherId: '',
    formattedOrigin: '',
    scopeExtensions: [],
    supportedLocales: [],
    isPinned: null,
    isPolicyPinned: null,
    selectedLocale: null,
    showSystemNotificationsSettingsLink: false,
    allowUninstall: false,
  };
}
