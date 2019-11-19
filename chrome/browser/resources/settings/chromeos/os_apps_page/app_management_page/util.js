// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for the App Management page.
 */

cr.define('app_management.util', function() {
  /**
   * @return {!AppManagementPageState}
   */
  function createEmptyState() {
    return {
      apps: {},
      arcSupported: false,
      selectedAppId: null,
    };
  }

  /**
   * @param {!Array<App>} apps
   * @return {!AppManagementPageState}
   */
  function createInitialState(apps) {
    const initialState = createEmptyState();

    initialState.arcSupported =
        loadTimeData.valueExists('isSupportedArcVersion') &&
        loadTimeData.getBoolean('isSupportedArcVersion');

    for (const app of apps) {
      initialState.apps[app.id] = app;
    }

    return initialState;
  }

  /**
   * @param {number} permissionId
   * @param {!PermissionValueType} valueType
   * @param {number} value
   * @param {boolean} isManaged
   * @return {!Permission}
   */
  function createPermission(permissionId, valueType, value, isManaged) {
    return {
      permissionId,
      valueType,
      value,
      isManaged,
    };
  }

  /**
   * @param {App} app
   * @return {string}
   */
  function getAppIcon(app) {
    return `chrome://app-icon/${app.id}/128`;
  }

  /**
   * If the given value is not in the set, returns a new set with the value
   * added, otherwise returns the old set.
   * @template T
   * @param {!Set<T>} set
   * @param {T} value
   * @return {!Set<T>}
   */
  function addIfNeeded(set, value) {
    if (!set.has(value)) {
      set = new Set(set);
      set.add(value);
    }
    return set;
  }

  /**
   * If the given value is in the set, returns a new set without the value,
   * otherwise returns the old set.
   * @template T
   * @param {!Set<T>} set
   * @param {T} value
   * @return {!Set<T>}
   */
  function removeIfNeeded(set, value) {
    if (set.has(value)) {
      set = new Set(set);
      set.delete(value);
    }
    return set;
  }

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  function getPermissionValueBool(app, permissionType) {
    const permission = getPermission(app, permissionType);
    assert(permission);

    switch (permission.valueType) {
      case PermissionValueType.kBool:
        return permission.value === Bool.kTrue;
      case PermissionValueType.kTriState:
        return permission.value === TriState.kAllow;
      default:
        assertNotReached();
    }
  }

  /**
   * Undefined is returned when the app does not request a permission.
   *
   * @param {App} app
   * @param {string} permissionType
   * @return {Permission|undefined}
   */
  function getPermission(app, permissionType) {
    return app.permissions[permissionTypeHandle(app, permissionType)];
  }

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {number}
   */
  function permissionTypeHandle(app, permissionType) {
    switch (app.type) {
      case AppType.kWeb:
        return PwaPermissionType[permissionType];
      case AppType.kArc:
        return ArcPermissionType[permissionType];
      default:
        assertNotReached();
    }
  }

  /**
   * @param {AppManagementPageState} state
   * @return {?App}
   */
  function getSelectedApp(state) {
    const selectedAppId = state.selectedAppId;
    return selectedAppId ? state.apps[selectedAppId] : null;
  }

  /**
   * A comparator function to sort strings alphabetically.
   *
   * @param {string} a
   * @param {string} b
   */
  function alphabeticalSort(a, b) {
    return a.localeCompare(b);
  }

  /**
   * Toggles an OptionalBool
   *
   * @param {OptionalBool} bool
   * @return {OptionalBool}
   */
  function toggleOptionalBool(bool) {
    switch (bool) {
      case OptionalBool.kFalse:
        return OptionalBool.kTrue;
      case OptionalBool.kTrue:
        return OptionalBool.kFalse;
      default:
        assertNotReached();
    }
  }

  /**
   * @param {OptionalBool} optionalBool
   * @returns {boolean}
   */
  function convertOptionalBoolToBool(optionalBool) {
    switch (optionalBool) {
      case OptionalBool.kTrue:
        return true;
      case OptionalBool.kFalse:
        return false;
      default:
        assertNotReached();
    }
  }

  /**
   * Navigates to the App Detail page.
   *
   * @param {string} appId
   */
  function openAppDetailPage(appId) {
    const params = new URLSearchParams;
    params.append('id', appId);
    settings.navigateTo(settings.routes.APP_MANAGEMENT_DETAIL, params);
  }

  /**
   * Navigates to the main App Management list page.
   */
  function openMainPage() {
    settings.navigateTo(settings.routes.APP_MANAGEMENT);
  }

  /**
   * @param {AppType} appType
   * @return {string}
   * @private
   */
  function getUserActionHistogramNameForAppType_(appType) {
    switch (appType) {
      case AppType.kArc:
        return 'AppManagement.AppDetailViews.ArcApp';
      case AppType.kExtension:
        return 'AppManagement.AppDetailViews.ChromeApp';
      case AppType.kWeb:
        return 'AppManagement.AppDetailViews.WebApp';
      default:
        assertNotReached();
    }
  }

  /**
   * @param {AppType} appType
   * @param {AppManagementUserAction} userAction
   */
  function recordAppManagementUserAction(appType, userAction) {
    const histogram = getUserActionHistogramNameForAppType_(appType);
    const enumLength = Object.keys(AppManagementUserAction).length;
    chrome.metricsPrivate.recordEnumerationValue(
        histogram, userAction, enumLength);
  }

  return {
    addIfNeeded: addIfNeeded,
    alphabeticalSort: alphabeticalSort,
    convertOptionalBoolToBool: convertOptionalBoolToBool,
    createEmptyState: createEmptyState,
    createInitialState: createInitialState,
    createPermission: createPermission,
    getAppIcon: getAppIcon,
    getPermission: getPermission,
    getPermissionValueBool: getPermissionValueBool,
    getSelectedApp: getSelectedApp,
    openAppDetailPage: openAppDetailPage,
    openMainPage: openMainPage,
    permissionTypeHandle: permissionTypeHandle,
    recordAppManagementUserAction: recordAppManagementUserAction,
    removeIfNeeded: removeIfNeeded,
    toggleOptionalBool: toggleOptionalBool,
  };
});
