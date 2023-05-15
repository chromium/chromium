// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This is the single source of truth for top-level page availability in
 * ChromeOS Settings. An available page is one reachable to the user via some
 * UI action. A page can be (un)available based on guest mode and/or enabled
 * features.
 *
 * NOTE: This is separate from page visibility, which deals with what pages are
 * visible to the user. For example, after removing infinite scroll b/272139876
 * the bluetooth page can be available/accessible, but might not be the active
 * visible page.
 */

import {isGuest, isKerberosEnabled, isPowerwashAllowed} from './common/load_time_booleans.js';
import {OsPageAvailability} from './mojom-webui/routes.mojom-webui.js';

export {OsPageAvailability};

/**
 * Used to create the pageAvailability object.
 *
 * Can be used to create the pageAvailability object with expected values after
 * overriding load time data within tests.
 */
export function createPageAvailability(): OsPageAvailability {
  if (isGuest()) {
    return {
      apps: true,
      bluetooth: true,
      crostini: true,
      dateTime: true,
      device: true,
      files: false,
      internet: true,
      kerberos: isKerberosEnabled(),
      multidevice: false,
      osAccessibility: true,
      osLanguages: true,
      osPeople: false,
      osPrinting: true,
      osPrivacy: true,
      osReset: isPowerwashAllowed(),
      osSearch: true,
      personalization: false,
    };
  }
  return {
    apps: true,
    bluetooth: true,
    crostini: true,
    dateTime: true,
    device: true,
    files: true,
    internet: true,
    kerberos: isKerberosEnabled(),
    multidevice: true,
    osAccessibility: true,
    osLanguages: true,
    osPeople: true,
    osPrinting: true,
    osPrivacy: true,
    osReset: isPowerwashAllowed(),
    osSearch: true,
    personalization: true,
  };
}
