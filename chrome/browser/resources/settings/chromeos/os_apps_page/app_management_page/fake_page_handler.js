// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PermissionType, PermissionValue, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType, InstallReason, InstallSource, OptionalBool, WindowMode} from 'chrome://resources/cr_components/app_management/constants.js';
import {createBoolPermission, createTriStatePermission, getTriStatePermissionValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {AppManagementStore} from './store.js';

/**
 * @implements {appManagement.mojom.PageHandlerInterface}
 */
export class FakePageHandler {
  /**
   * @param {Object=} options
   * @return {!Object<number, appManagement.mojom.Permission>}
   */
  static createWebPermissions(options) {
    const permissionTypes = [
      PermissionType.kLocation,
      PermissionType.kNotifications,
      PermissionType.kMicrophone,
      PermissionType.kCamera,
    ];

    const permissions = {};

    for (const permissionType of permissionTypes) {
      let permissionValue = TriState.kAllow;
      let isManaged = false;

      if (options && options[permissionType]) {
        const opts = options[permissionType];
        permissionValue =
            getTriStatePermissionValue(opts.value) || permissionValue;
        isManaged = opts.isManaged || isManaged;
      }
      permissions[permissionType] =
          createTriStatePermission(permissionType, permissionValue, isManaged);
    }

    return permissions;
  }

  /**
   * @param {Array<number>=} optIds
   * @return {!Object<number, appManagement.mojom.Permission>}
   */
  static createArcPermissions(optIds) {
    const permissionTypes = optIds || [
      PermissionType.kCamera,
      PermissionType.kLocation,
      PermissionType.kMicrophone,
      PermissionType.kNotifications,
      PermissionType.kContacts,
      PermissionType.kStorage,
    ];

    const permissions = {};

    for (const permissionType of permissionTypes) {
      permissions[permissionType] =
          createBoolPermission(permissionType, true, false /*is_managed*/);
    }

    return permissions;
  }

  /**
   * @param {appManagement.mojom.AppType} appType
   * @return {!Object<number, appManagement.mojom.Permission>}
   */
  static createPermissions(appType) {
    switch (appType) {
      case (AppType.kWeb):
        return FakePageHandler.createWebPermissions();
      case (AppType.kArc):
        return FakePageHandler.createArcPermissions();
      default:
        return {};
    }
  }

  /**
   * @param {string} id
   * @param {Object=} optConfig
   * @return {!App}
   */
  static createApp(id, optConfig) {
    const app = {
      id: id,
      type: AppType.kWeb,
      title: 'App Title',
      description: '',
      version: '5.1',
      size: '9.0MB',
      isPinned: OptionalBool.kFalse,
      isPolicyPinned: OptionalBool.kFalse,
      installReason: InstallReason.kUser,
      permissions: {},
      hideMoreSettings: false,
      hidePinToShelf: false,
      isPreferredApp: false,
      windowMode: WindowMode.kWindow,
      hideWindowMode: false,
      resizeLocked: false,
      hideResizeLocked: true,
      supportedLinks: [],
      runOnOsLogin: null,
      fileHandlingState: null,
      installSource: InstallSource.kUnknown,
      appSize: '',
      dataSize: '',
      publisherId: '',
    };

    if (optConfig) {
      Object.assign(app, optConfig);
    }

    // Only create default permissions if none were provided in the config.
    if (!optConfig || optConfig.permissions === undefined) {
      app.permissions = FakePageHandler.createPermissions(app.type);
    }

    return app;
  }

  /**
   * @param {appManagement.mojom.PageRemote} page
   */
  constructor(page) {
    this.receiver_ = new appManagement.mojom.PageHandlerReceiver(this);
    /** @type {appManagement.mojom.PageRemote} */
    this.page = page;

    /** @type {!Array<App>} */
    this.apps_ = [];

    /** @type {Array<!string>} */
    this.overlappingAppIds = [];

    /** @type {number} */
    this.guid = 0;

    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();
    this.resolverMap_.set('setPreferredApp', new PromiseResolver());
    this.resolverMap_.set('getOverlappingPreferredApps', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    const method = this.resolverMap_.get(methodName);
    assert(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  /**
   * @param {string} methodName
   * @protected
   */
  methodCalled(methodName) {
    this.getResolver_(methodName).resolve();
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.getResolver_(methodName).promise.then(() => {
      // Support sequential calls to whenCalled by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /**
   * @returns {!appManagement.mojom.PageHandlerRemote}
   */
  getRemote() {
    return this.receiver_.$.bindNewPipeAndPassRemote();
  }

  async flushPipesForTesting() {
    await this.page.$.flushForTesting();
  }

  /**
   * @return {!Promise<{apps: !Array<!appManagement.mojom.App>}>}
   */
  async getApps() {
    return {apps: this.apps_};
  }

  /**
   * @param {!string} appId
   * @return {!Promise<{app: appManagement.mojom.App}>}
   */
  async getApp(appId) {
    assertNotReached();
  }

  /**
   * @param {!string} appId
   * @return {!Promise<{messages:
   *     !Array<!appManagement.mojom.ExtensionAppPermissionMessage>}>}
   */
  async getExtensionAppPermissionMessages(appId) {
    return {messages: []};
  }

  /**
   * @param {!Array<App>} appList
   */
  setApps(appList) {
    this.apps_ = appList;
  }

  /**
   * @param {string} appId
   * @param {appManagement.mojom.OptionalBool} pinnedValue
   */
  setPinned(appId, pinnedValue) {
    const app = AppManagementStore.getInstance().data.apps[appId];

    const newApp =
        /** @type {!App} */ (Object.assign({}, app, {isPinned: pinnedValue}));
    this.page.onAppChanged(newApp);
  }

  /**
   * @param {string} appId
   * @param {appManagement.mojom.Permission} permission
   */
  setPermission(appId, permission) {
    const app = AppManagementStore.getInstance().data.apps[appId];

    // Check that the app had a previous value for the given permission
    assert(app.permissions[permission.permissionType]);

    const newPermissions = Object.assign({}, app.permissions);
    newPermissions[permission.permissionType] = permission;
    const newApp = /** @type {!App} */ (
        Object.assign({}, app, {permissions: newPermissions}));
    this.page.onAppChanged(newApp);
  }

  /**
   * @param {string} appId
   * @param {boolean} locked
   */
  setResizeLocked(appId, locked) {
    const app = AppManagementStore.getInstance().data.apps[appId];

    const newApp =
        /** @type {!App} */ (Object.assign({}, app, {resizeLocked: locked}));
    this.page.onAppChanged(newApp);
  }

  /**
   * @param {string} appId
   * @param {boolean} hide
   */
  setHideResizeLocked(appId, hide) {
    const app = AppManagementStore.getInstance().data.apps[appId];

    const newApp =
        /** @type {!App} */ (Object.assign({}, app, {hideResizeLocked: hide}));
    this.page.onAppChanged(newApp);
  }

  /**
   * @param {string} appId
   */
  uninstall(appId) {
    this.page.onAppRemoved(appId);
  }

  /**
   * @param {string} appId
   * @param {boolean} preferredAppValue
   */
  setPreferredApp(appId, preferredAppValue) {
    const app = AppManagementStore.getInstance().data.apps[appId];

    const newApp =
        /** @type {!App} */ (
            Object.assign({}, app, {isPreferredApp: preferredAppValue}));
    this.page.onAppChanged(newApp);
    this.methodCalled('setPreferredApp');
  }

  /**
   * @param {string} appId
   */
  openNativeSettings(appId) {}

  /**
   * @param {string} appId
   * @param {appManagement.mojom.WindowMode} windowMode
   */
  setWindowMode(appId, windowMode) {
    assertNotReached();
  }

  /**
   * @param {string} appId
   * @param {appManagement.mojom.RunOnOsLoginMode} runOnOsLoginMode
   */
  setRunOnOsLoginMode(appId, runOnOsLoginMode) {
    assertNotReached();
  }

  /**
   * @param {string} appId
   * @param {boolean} fileHandlingEnabled
   */
  setFileHandlingEnabled(appId, fileHandlingEnabled) {
    assertNotReached();
  }

  showDefaultAppAssociationsUi() {
    assertNotReached();
  }

  /**
   * @param {string} appId
   * @return {!Promise<{ appIds: !Array<!string> }>}
   */
  async getOverlappingPreferredApps(appId) {
    this.methodCalled('getOverlappingPreferredApps');
    if (!this.overlappingAppIds) {
      return {appIds: []};
    }
    return {appIds: this.overlappingAppIds};
  }

  /**
   * @param {string} appId
   */
  openStorePage(appId) {}

  /**
   * @param {string} optId
   * @param {Object=} optConfig
   * @return {!Promise<!App>}
   */
  async addApp(optId, optConfig) {
    optId = optId || String(this.guid++);
    const app = FakePageHandler.createApp(optId, optConfig);
    this.page.onAppAdded(app);
    await this.flushPipesForTesting();
    return app;
  }

  /**
   * Takes an app id and an object mapping app fields to the values they
   * should be changed to, and dispatches an action to carry out these
   * changes.
   * @param {string} id
   * @param {Object} changes
   */
  async changeApp(id, changes) {
    this.page.onAppChanged(FakePageHandler.createApp(id, changes));
    await this.flushPipesForTesting();
  }
}
