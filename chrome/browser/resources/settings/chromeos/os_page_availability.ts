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

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

export interface OsPageAvailability {
  a11y: boolean;
  apps: boolean;
  bluetooth: boolean;
  crostini: boolean;
  dateTime: boolean;
  device: boolean;
  files: boolean;
  internet: boolean;
  kerberos: boolean;
  languages: boolean;
  multidevice: boolean;
  people: boolean|{
    googleAccounts: boolean,
    lockScreen: boolean,
  };
  personalization: boolean;
  printing: boolean;
  privacy: boolean;
  reset: boolean;
  search: boolean;
}

/**
 * Used to create the pageAvailability object.
 *
 * Can be used to create the pageAvailability object with expected values after
 * overriding load time data within tests.
 */
export function createPageAvailability(): OsPageAvailability {
  const isGuestMode = loadTimeData.getBoolean('isGuest');
  const isAccountManagerEnabled =
      loadTimeData.valueExists('isAccountManagerEnabled') &&
      loadTimeData.getBoolean('isAccountManagerEnabled');
  const isKerberosEnabled = loadTimeData.valueExists('isKerberosEnabled') &&
      loadTimeData.getBoolean('isKerberosEnabled');
  const isPowerwashAllowed = loadTimeData.getBoolean('allowPowerwash');

  if (isGuestMode) {
    return {
      a11y: true,
      apps: true,
      bluetooth: true,
      crostini: true,
      dateTime: true,
      device: true,
      files: false,
      internet: true,
      kerberos: isKerberosEnabled,
      languages: true,
      multidevice: false,
      people: false,
      personalization: false,
      printing: true,
      privacy: true,
      reset: isPowerwashAllowed,
      search: true,
    };
  }
  return {
    a11y: true,
    apps: true,
    bluetooth: true,
    crostini: true,
    dateTime: true,
    device: true,
    files: true,
    internet: true,
    kerberos: isKerberosEnabled,
    languages: true,
    multidevice: true,
    people: {
      googleAccounts: isAccountManagerEnabled,
      lockScreen: true,
    },
    personalization: true,
    printing: true,
    privacy: true,
    reset: isPowerwashAllowed,
    search: true,
  };
}
