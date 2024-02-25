// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This is the single source of truth for top-level page availability in
 * ChromeOS Settings. An available page is one reachable to the user (ie. the
 * route exists). A page may be (un)available based on guest mode and/or enabled
 * features.
 *
 * NOTE: This is separate from page visibility, which deals with what pages are
 * visible to the user. For example, after removing infinite scroll b/272139876
 * the bluetooth page can be available/accessible, but might not be the active
 * visible page.
 */

import {Section} from './mojom-webui/routes.mojom-webui.js';
import {routes} from './router.js';

/**
 * Defines which top-level pages/sections are available to the user. Page keys
 * should must derive from the Section enum in routes.mojom.
 */
export type OsPageAvailability = Record<Section, boolean>;

/**
 * Used to create the page availability object depending on route existence.
 * For example, the kAboutChromeOs page should be available if the corresponding
 * ABOUT route exists.
 *
 * Note: This function can be used in tests to create the page availability
 * object with expected values after overriding the set of available routes.
 */
export function createPageAvailability(): OsPageAvailability {
  return {
    [Section.kAboutChromeOs]: !!routes.ABOUT,
    [Section.kAccessibility]: !!routes.OS_ACCESSIBILITY,
    [Section.kApps]: !!routes.APPS,
    [Section.kBluetooth]: !!routes.BLUETOOTH,
    [Section.kDevice]: !!routes.DEVICE,
    [Section.kKerberos]: !!routes.KERBEROS,
    [Section.kMultiDevice]: !!routes.MULTIDEVICE,
    [Section.kNetwork]: !!routes.INTERNET,
    [Section.kPeople]: !!routes.OS_PEOPLE,
    [Section.kPersonalization]: !!routes.PERSONALIZATION,
    [Section.kPrivacyAndSecurity]: !!routes.OS_PRIVACY,

    // Only available when OsSettingsRevampWayfinding feature is enabled.
    [Section.kSystemPreferences]: !!routes.SYSTEM_PREFERENCES,

    // Only available when OsSettingsRevampWayfinding feature is disabled.
    [Section.kCrostini]: !!routes.CROSTINI,
    [Section.kDateAndTime]: !!routes.DATETIME,
    [Section.kFiles]: !!routes.FILES,
    [Section.kLanguagesAndInput]: !!routes.OS_LANGUAGES,
    [Section.kPrinting]: !!routes.OS_PRINTING,
    [Section.kReset]: !!routes.OS_RESET,
    [Section.kSearchAndAssistant]: !!routes.OS_SEARCH,
  };
}
