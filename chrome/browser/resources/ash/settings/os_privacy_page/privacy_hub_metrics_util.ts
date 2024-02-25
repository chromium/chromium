// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const CAMERA_SUBPAGE_USER_ACTION_HISTOGRAM_NAME =
    'ChromeOS.PrivacyHub.CameraSubpage.UserAction';
export const MICROPHONE_SUBPAGE_USER_ACTION_HISTOGRAM_NAME =
    'ChromeOS.PrivacyHub.MicrophoneSubpage.UserAction';
export const LOCATION_SUBPAGE_USER_ACTION_HISTOGRAM_NAME =
    'ChromeOS.PrivacyHub.LocationSubpage.UserAction';

export const LOCATION_PERMISSION_CHANGE_FROM_SETTINGS_HISTOGRAM_NAME =
    'ChromeOS.PrivacyHub.Geolocation.AccessLevelChanged.SystemSettings';
export const LOCATION_PERMISSION_CHANGE_FROM_DIALOG_HISTOGRAM_NAME =
    'ChromeOS.PrivacyHub.Geolocation.AccessLevelChanged.GeolocationDialog';
export const LOCATION_PERMISSION_CHANGE_FROM_NOTIFICATION_HISTOGRAM_NAME =
    'ChromeOS.PrivacyHub.Geolocation.AccessLevelChanged.' +
    'LocationPermissionNotification';
/**
 * Enumeration of the user actions that can be taken on the Privacy Hub sensor
 * subpages.
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/metadata/chromeos/enums.xml, and should always
 * reflect it (do not change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */

export enum PrivacyHubSensorSubpageUserAction {
  SUBPAGE_OPENED = 0,
  SYSTEM_ACCESS_CHANGED = 1,
  APP_PERMISSION_CHANGED = 2,
  ANDROID_SETTINGS_LINK_CLICKED = 3,
  WEBSITE_PERMISSION_LINK_CLICKED = 4,
}

export const NUMBER_OF_POSSIBLE_USER_ACTIONS =
    Object.keys(PrivacyHubSensorSubpageUserAction).length;
