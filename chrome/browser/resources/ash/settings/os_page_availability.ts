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
import {Section} from './mojom-webui/routes.mojom-webui.js';

/**
 * Defines which top-level pages/sections are available to the user. Page keys
 * should must derive from the Section enum in routes.mojom.
 */
export type OsPageAvailability = Record<Section, boolean>;

/**
 * Used to create the pageAvailability object depending on load time data.
 * Can be used to create the pageAvailability object with expected values after
 * overriding load time data within tests.
 */
export function createPageAvailability(): OsPageAvailability {
  const isGuestMode = isGuest();

  return {
    [Section.kAboutChromeOs]: true,
    [Section.kAccessibility]: true,
    [Section.kApps]: true,
    [Section.kBluetooth]: true,
    [Section.kCrostini]: true,
    [Section.kDateAndTime]: true,
    [Section.kDevice]: true,
    [Section.kFiles]: !isGuestMode,
    [Section.kKerberos]: isKerberosEnabled(),
    [Section.kLanguagesAndInput]: true,
    [Section.kMultiDevice]: !isGuestMode,
    [Section.kNetwork]: true,
    [Section.kPeople]: !isGuestMode,
    [Section.kPersonalization]: !isGuestMode,
    [Section.kPrinting]: true,
    [Section.kPrivacyAndSecurity]: true,
    [Section.kReset]: isPowerwashAllowed(),
    [Section.kSearchAndAssistant]: true,
  };
}
