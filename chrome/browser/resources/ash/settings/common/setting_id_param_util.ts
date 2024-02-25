// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility function for retrieving the deep linked setting ID
 * from the Url parameter.
 */

import {Router} from '../router.js';

const SETTING_ID_URL_PARAM_NAME = 'settingId';

/**
 * Retrieves the setting ID saved in the URL's query parameter. Returns null if
 * setting ID is unavailable.
 */
export function getSettingIdParameter(): string|null {
  return Router.getInstance().getQueryParameters().get(
      SETTING_ID_URL_PARAM_NAME);
}
