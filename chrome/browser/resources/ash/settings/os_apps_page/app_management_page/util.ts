// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {App, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {getPermission, getPermissionValueAsTriState} from 'chrome://resources/cr_components/app_management/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {Router, routes} from '../../router.js';

/**
 * Navigates to the App Detail page.
 */
export function openAppDetailPage(appId: string): void {
  const params = new URLSearchParams();
  params.append('id', appId);
  Router.getInstance().navigateTo(routes.APP_MANAGEMENT_DETAIL, params);
}

/**
 * Navigates to the main App Management list page.
 */
export function openMainPage(): void {
  Router.getInstance().navigateTo(routes.APP_MANAGEMENT);
}

/**
 * @param isSensorBlocked - Access to Camera, Microphone and Location can be
 * blocked system wide from privacy hub. This input parameter indicates whether
 * the relevant sensor is blocked.
 */
export function getPermissionDescriptionString(
    app: App|undefined, permissionType: PermissionTypeIndex|undefined,
    isSensorBlocked: boolean): string {
  if (app === undefined || permissionType === undefined) {
    return '';
  }

  const permission = getPermission(app, permissionType);
  assert(permission);

  const value = getPermissionValueAsTriState(app, permissionType);

  if (value === TriState.kAllow && isSensorBlocked) {
    if (PermissionType[permissionType] === PermissionType.kCamera) {
      return permission.details ?
          loadTimeData.getStringF(
              'permissionAllowedTextWithDetailsAndTurnOnCameraAccessButton',
              permission.details) :
          loadTimeData.getString(
              'permissionAllowedTextWithTurnOnCameraAccessButton');
    } else if (PermissionType[permissionType] === PermissionType.kLocation) {
      return permission.details ?
          loadTimeData.getStringF(
              'permissionAllowedTextWithDetailsAndTurnOnLocationAccessButton',
              permission.details) :
          loadTimeData.getString(
              'permissionAllowedTextWithTurnOnLocationAccessButton');
    } else if (PermissionType[permissionType] === PermissionType.kMicrophone) {
      return permission.details ?
          loadTimeData.getStringF(
              'permissionAllowedTextWithDetailsAndTurnOnMicrophoneAccessButton',
              permission.details) :
          loadTimeData.getString(
              'permissionAllowedTextWithTurnOnMicrophoneAccessButton');
    }
  }

  if (value === TriState.kAllow && permission.details) {
    return loadTimeData.getStringF(
        'appManagementPermissionAllowedWithDetails', permission.details);
  }

  switch (value) {
    case TriState.kAllow:
      return loadTimeData.getString('appManagementPermissionAllowed');
    case TriState.kBlock:
      return loadTimeData.getString('appManagementPermissionDenied');
    case TriState.kAsk:
      return loadTimeData.getString('appManagementPermissionAsk');
  }
}
