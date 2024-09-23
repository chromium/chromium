// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppParentalControlsHandler, AppParentalControlsHandlerInterface} from '../../mojom-webui/app_parental_controls_handler.mojom-webui.js';

let appParentalControlsProvider: AppParentalControlsHandlerInterface|null =
    null;

export function setAppParentalControlsProviderForTesting(
    testProvider: AppParentalControlsHandlerInterface): void {
  appParentalControlsProvider = testProvider;
}

export function getAppParentalControlsProvider():
    AppParentalControlsHandlerInterface {
  // For testing only.
  if (appParentalControlsProvider) {
    return appParentalControlsProvider;
  }
  appParentalControlsProvider = AppParentalControlsHandler.getRemote();
  return appParentalControlsProvider;
}
