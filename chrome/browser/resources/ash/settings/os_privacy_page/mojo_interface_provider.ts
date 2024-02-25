// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppPermissionsHandler, AppPermissionsHandlerInterface} from '../mojom-webui/app_permission_handler.mojom-webui.js';

let appPermissionProvider: AppPermissionsHandlerInterface|null = null;

export function setAppPermissionProviderForTesting(
    fakeProvider: AppPermissionsHandlerInterface): void {
  appPermissionProvider = fakeProvider;
}

export function getAppPermissionProvider(): AppPermissionsHandlerInterface {
  if (appPermissionProvider === null) {
    appPermissionProvider = AppPermissionsHandler.getRemote();
  }
  return appPermissionProvider;
}
