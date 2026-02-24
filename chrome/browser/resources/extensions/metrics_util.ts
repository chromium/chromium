// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Shared constants and enums used for logging user actions and
 * histograms in the extensions management page.
 */

export enum UserAction {
  ALL_TOGGLED_ON = 'Extensions.Settings.HostList.AllHostsToggledOn',
  ALL_TOGGLED_OFF = 'Extensions.Settings.HostList.AllHostsToggledOff',
  SPECIFIC_TOGGLED_ON = 'Extensions.Settings.HostList.SpecificHostToggledOn',
  SPECIFIC_TOGGLED_OFF = 'Extensions.Settings.HostList.SpecificHostToggledOff',
  LEARN_MORE = 'Extensions.Settings.HostList.LearnMoreActivated',
}

// Values for logging Extension Safety Hub metrics.
export const SAFETY_HUB_EXTENSION_KEPT_HISTOGRAM_NAME =
    'SafeBrowsing.ExtensionSafetyHub.Trigger.Kept';
export const SAFETY_HUB_EXTENSION_REMOVED_HISTOGRAM_NAME =
    'SafeBrowsing.ExtensionSafetyHub.Trigger.Removed';
export const SAFETY_HUB_EXTENSION_SHOWN_HISTOGRAM_NAME =
    'SafeBrowsing.ExtensionSafetyHub.Trigger.Shown';

// This number should match however many entries are defined in the
// `SafetyCheckWarningReason` defined in the `enums.xml` file.
export const SAFETY_HUB_WARNING_REASON_MAX_SIZE = 7;

// Histogram names for logging when an extension is uploaded to the user's
// account.
export const UPLOAD_EXTENSION_TO_ACCOUNT_ITEMS_LIST_PAGE_HISTOGRAM_NAME =
    'Extensions.UploadExtensionToAccount.ItemsListPage';
export const UPLOAD_EXTENSION_TO_ACCOUNT_DETAILS_VIEW_PAGE_HISTOGRAM_NAME =
    'Extensions.UploadExtensionToAccount.DetailsViewPage';
