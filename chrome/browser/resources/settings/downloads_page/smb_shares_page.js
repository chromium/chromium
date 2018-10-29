// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-smb-shares-page',

  behaviors: [
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    showAddSmbDialog_: Boolean,

    /** @private */
    addShareResultText_: String,
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route == settings.routes.SMB_SHARES) {
      this.showAddSmbDialog_ =
          settings.getQueryParameters().get('showAddShare') == 'true';
    }
  },

  /** @override */
  attached: function() {
    this.addWebUIListener('on-add-smb-share', this.onAddShare_.bind(this));
  },

  /** @private */
  onAddShareTap_: function() {
    this.showAddSmbDialog_ = true;
  },

  /** @private */
  onAddSmbDialogClosed_: function() {
    this.showAddSmbDialog_ = false;
  },

  /**
   * @param {SmbMountResult} result
   * @private
   */
  onAddShare_: function(result) {
    switch (result) {
      case SmbMountResult.SUCCESS:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedSuccessfulMessage');
        break;
      case SmbMountResult.AUTHENTICATION_FAILED:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedAuthFailedMessage');
        break;
      case SmbMountResult.NOT_FOUND:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedNotFoundMessage');
        break;
      case SmbMountResult.UNSUPPORTED_DEVICE:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedUnsupportedDeviceMessage');
        break;
      case SmbMountResult.MOUNT_EXISTS:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedMountExistsMessage');
        break;
      case SmbMountResult.INVALID_URL:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedInvalidURLMessage');
        break;
      default:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedErrorMessage');
    }
    this.$.errorToast.show();
  },

});
