// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility function for retrieving the deep linked setting ID
 * from the Url parameter.
 */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Router} from './router.js';

/** @type {string} */
const SETTING_ID_URL_PARAM_NAME = 'settingId';

/**
 * Retrieves the setting ID saved in the URL's query parameter. Returns null if
 * setting ID is unavailable.
 * @return {?string}
 */
export function getSettingIdParameter() {
  // This flag must be enabled for the setting ID to be available.
  if (!loadTimeData.valueExists('isDeepLinkingEnabled') ||
      !loadTimeData.getBoolean('isDeepLinkingEnabled')) {
    return null;
  }

  return Router.getInstance().getQueryParameters().get(
      SETTING_ID_URL_PARAM_NAME);
}
