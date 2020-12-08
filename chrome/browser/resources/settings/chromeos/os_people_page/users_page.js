// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-page' is the settings page for managing user accounts on
 * the device.
 */
Polymer({
  is: 'settings-users-page',

  behaviors: [
    DeepLinkingBehavior,
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
    isOwner_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    isUserListManaged_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isChild_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isSupervised');
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kGuestBrowsing,
        chromeos.settings.mojom.Setting.kShowUsernamesAndPhotosAtSignIn,
        chromeos.settings.mojom.Setting.kRestrictSignIn,
        chromeos.settings.mojom.Setting.kAddToUserAllowlist,
        chromeos.settings.mojom.Setting.kRemoveFromUserAllowlist,

        chromeos.settings.mojom.Setting.kGuestBrowsingV2,
        chromeos.settings.mojom.Setting.kShowUsernamesAndPhotosAtSignInV2,
        chromeos.settings.mojom.Setting.kRestrictSignInV2,
        chromeos.settings.mojom.Setting.kAddToUserAllowlistV2,
        chromeos.settings.mojom.Setting.kRemoveFromUserAllowlistV2,
      ]),
    },

    /**
     * True if redesign of account management flows is enabled.
     * @private
     */
    isAccountManagementFlowsV2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled');
      },
      readOnly: true,
    },
  },

  listeners: {'all-managed-users-removed': 'focusAddUserButton_'},

  /** @override */
  created() {
    chrome.usersPrivate.getCurrentUser(user => {
      this.isOwner_ = user.isOwner;
    });

    chrome.usersPrivate.isUserListManaged(isUserListManaged => {
      this.isUserListManaged_ = isUserListManaged;
    });
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId !==
            chromeos.settings.mojom.Setting.kRemoveFromUserAllowlist &&
        settingId !==
            chromeos.settings.mojom.Setting.kRemoveFromUserAllowlistV2) {
      // Continue with deep linking attempt.
      return true;
    }

    // Wait for element to load.
    Polymer.RenderStatus.afterNextRender(this, () => {
      const userList = this.$$('settings-user-list');
      const removeButton = userList.$$('cr-icon-button');
      if (removeButton) {
        this.showDeepLinkElement(removeButton);
        return;
      }
      console.warn(`Element with deep link id ${settingId} not focusable.`);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.ACCOUNTS) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {!Event} e
   * @private
   */
  openAddUserDialog_(e) {
    e.preventDefault();
    this.$.addUserDialog.open();
  },

  /** @private */
  onAddUserDialogClose_() {
    this.focusAddUserButton_();
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isUserListManaged
   * @private
   * @return {boolean}
   */
  isEditingDisabled_(isOwner, isUserListManaged) {
    return !isOwner || isUserListManaged;
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isUserListManaged
   * @param {boolean} allowGuest
   * @param {boolean} isChild
   * @private
   * @return {boolean}
   */
  isEditingUsersEnabled_(isOwner, isUserListManaged, allowGuest, isChild) {
    return isOwner && !isUserListManaged && !allowGuest && !isChild;
  },

  /** @return {boolean} */
  shouldHideModifiedByOwnerLabel_() {
    return this.isUserListManaged_ || this.isOwner_;
  },

  /** @private */
  focusAddUserButton_() {
    cr.ui.focusWithoutInk(assert(this.$$('#add-user-button a')));
  },
});
