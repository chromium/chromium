// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-permission-item',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * The name of the permission, to be displayed to the user.
     * @type {string}
     */
    permissionLabel: String,

    /**
     * A string version of the permission type. Must be a value of the
     * permission type enum corresponding to the AppType of app_.
     * E.g. A value of PwaPermissionType if app_.type === AppType.kWeb.
     * @type {string}
     */
    permissionType: String,

    /**
     * @type {string}
     */
    icon: String,

    /**
     * @type {App}
     */
    app_: Object,

    /**
     * True if the permission type is available for the app.
     * @type {boolean}
     * @private
     */
    available_: {
      type: Boolean,
      computed: 'isAvailable_(app_, permissionType)',
      reflectToAttribute: true,
    },

    /**
     * @type {boolean}
     * @private
     */
    disabled_: {
      type: Boolean,
      computed: 'isManaged_(app_, permissionType)',
      reflectToAttribute: true,
    },
  },


  listeners: {click: 'onClick_', change: 'togglePermission_'},

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * Returns true if the permission type is available for the app.
   *
   * @param {App} app
   * @param {string} permissionType
   * @private
   */
  isAvailable_: function(app, permissionType) {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    assert(app);

    return app_management.util.getPermission(app, permissionType) !== undefined;
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  isManaged_: function(app, permissionType) {
    if (app === undefined || permissionType === undefined ||
        !this.isAvailable_(app, permissionType)) {
      return false;
    }

    assert(app);
    const permission = app_management.util.getPermission(app, permissionType);

    assert(permission);
    return permission.isManaged;
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  getValue_: function(app, permissionType) {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    assert(app);

    return app_management.util.getPermissionValueBool(app, permissionType);
  },

  /**
   * @private
   */
  onClick_: function() {
    this.$$('#toggle-row').click();
  },

  /**
   * @private
   */
  togglePermission_: function() {
    assert(this.app_);

    /** @type {!Permission} */
    let newPermission;

    let newBoolState = false;  // to keep the closure compiler happy.

    switch (app_management.util.getPermission(this.app_, this.permissionType)
                .valueType) {
      case PermissionValueType.kBool:
        newPermission =
            this.getNewPermissionBoolean_(this.app_, this.permissionType);
        newBoolState = newPermission.value === Bool.kTrue;
        break;
      case PermissionValueType.kTriState:
        newPermission =
            this.getNewPermissionTriState_(this.app_, this.permissionType);
        newBoolState = newPermission.value === TriState.kAllow;
        break;
      default:
        assertNotReached();
    }

    app_management.BrowserProxy.getInstance().handler.setPermission(
        this.app_.id, newPermission);

    app_management.util.recordAppManagementUserAction(
        this.app_.type,
        this.getUserMetricActionForPermission_(
            newBoolState, this.permissionType));
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {!Permission}
   * @private
   */
  getNewPermissionBoolean_: function(app, permissionType) {
    let newPermissionValue;
    const currentPermission =
        app_management.util.getPermission(app, permissionType);

    switch (currentPermission.value) {
      case Bool.kFalse:
        newPermissionValue = Bool.kTrue;
        break;
      case Bool.kTrue:
        newPermissionValue = Bool.kFalse;
        break;
      default:
        assertNotReached();
    }

    assert(newPermissionValue !== undefined);
    return app_management.util.createPermission(
        app_management.util.permissionTypeHandle(app, permissionType),
        PermissionValueType.kBool, newPermissionValue,
        currentPermission.isManaged);
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {!Permission}
   * @private
   */
  getNewPermissionTriState_: function(app, permissionType) {
    let newPermissionValue;
    const currentPermission =
        app_management.util.getPermission(app, permissionType);

    switch (currentPermission.value) {
      case TriState.kBlock:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAsk:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAllow:
        // TODO(rekanorman): Eventually TriState.kAsk, but currently changing a
        // permission to kAsk then opening the site settings page for the app
        // produces the error:
        // "Only extensions or enterprise policy can change the setting to ASK."
        newPermissionValue = TriState.kBlock;
        break;
      default:
        assertNotReached();
    }

    assert(newPermissionValue !== undefined);
    return app_management.util.createPermission(
        app_management.util.permissionTypeHandle(app, permissionType),
        PermissionValueType.kTriState, newPermissionValue,
        currentPermission.isManaged);
  },

  /**
   * @param {boolean} permissionValue
   * @param {string} permissionType
   * @return {AppManagementUserAction}
   * @private
   */
  getUserMetricActionForPermission_: function(permissionValue, permissionType) {
    switch (permissionType) {
      case 'NOTIFICATIONS':
        return permissionValue ? AppManagementUserAction.NotificationsTurnedOn :
                                 AppManagementUserAction.NotificationsTurnedOff;

      case 'GEOLOCATION':
      case 'LOCATION':
        return permissionValue ? AppManagementUserAction.LocationTurnedOn :
                                 AppManagementUserAction.LocationTurnedOff;

      case 'MEDIASTREAM_CAMERA':
      case 'CAMERA':
        return permissionValue ? AppManagementUserAction.CameraTurnedOn :
                                 AppManagementUserAction.CameraTurnedOff;

      case 'MEDIASTREAM_MIC':
      case 'MICROPHONE':
        return permissionValue ? AppManagementUserAction.MicrophoneTurnedOn :
                                 AppManagementUserAction.MicrophoneTurnedOff;

      case 'CONTACTS':
        return permissionValue ? AppManagementUserAction.ContactsTurnedOn :
                                 AppManagementUserAction.ContactsTurnedOff;

      case 'STORAGE':
        return permissionValue ? AppManagementUserAction.StorageTurnedOn :
                                 AppManagementUserAction.StorageTurnedOff;

      default:
        assertNotReached();
    }
  },
});
