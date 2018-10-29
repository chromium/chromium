// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-profile' is the settings subpage containing controls to
 * edit a profile's name, icon, and desktop shortcut.
 */
Polymer({
  is: 'settings-manage-profile',

  behaviors: [WebUIListenerBehavior, settings.RouteObserverBehavior],

  properties: {
    /**
     * The newly selected avatar. Populated only if the user manually changes
     * the avatar selection. The observer ensures that the changes are
     * propagated to the C++.
     * @private
     */
    profileAvatar_: {
      type: Object,
      observer: 'profileAvatarChanged_',
    },

    /**
     * The current profile name.
     */
    profileName: String,

    /**
     * True if the current profile has a shortcut.
     */
    hasProfileShortcut_: Boolean,

    /**
     * The available icons for selection.
     * @type {!Array<!AvatarIcon>}
     */
    availableIcons: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The current sync status.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * True if the profile shortcuts feature is enabled.
     */
    isProfileShortcutSettingVisible_: Boolean,
  },

  /** @private {?settings.ManageProfileBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.ManageProfileBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    const setIcons = icons => {
      this.availableIcons = icons;
    };

    this.addWebUIListener('available-icons-changed', setIcons);
    this.browserProxy_.getAvailableIcons().then(setIcons);
  },

  /** @protected */
  currentRouteChanged: function() {
    if (settings.getCurrentRoute() == settings.routes.MANAGE_PROFILE) {
      if (loadTimeData.getBoolean('profileShortcutsEnabled')) {
        this.browserProxy_.getProfileShortcutStatus().then(status => {
          if (status == ProfileShortcutStatus.PROFILE_SHORTCUT_SETTING_HIDDEN) {
            this.isProfileShortcutSettingVisible_ = false;
            return;
          }

          this.isProfileShortcutSettingVisible_ = true;
          this.hasProfileShortcut_ =
              status == ProfileShortcutStatus.PROFILE_SHORTCUT_FOUND;
        });
      }
    }
  },

  /**
   * Handler for when the profile name field is changed, then blurred.
   * @param {!Event} event
   * @private
   */
  onProfileNameChanged_: function(event) {
    if (event.target.invalid)
      return;

    this.browserProxy_.setProfileName(event.target.value);
  },

  /**
   * Handler for profile name keydowns.
   * @param {!Event} event
   * @private
   */
  onProfileNameKeydown_: function(event) {
    if (event.key == 'Escape') {
      event.target.value = this.profileName;
      event.target.blur();
    }
  },

  /**
   * Handler for when the profile avatar is changed by the user.
   * @private
   */
  profileAvatarChanged_: function() {
    if (this.profileAvatar_.isGaiaAvatar)
      this.browserProxy_.setProfileIconToGaiaAvatar();
    else
      this.browserProxy_.setProfileIconToDefaultAvatar(this.profileAvatar_.url);
  },

  /**
   * @param {?settings.SyncStatus} syncStatus
   * @return {boolean} Whether the profile name field is disabled.
   * @private
   */
  isProfileNameDisabled_: function(syncStatus) {
    return !!syncStatus.supervisedUser && !syncStatus.childUser;
  },

  /**
   * Handler for when the profile shortcut toggle is changed.
   * @param {!Event} event
   * @private
   */
  onHasProfileShortcutChange_: function(event) {
    if (this.hasProfileShortcut_) {
      this.browserProxy_.addProfileShortcut();
    } else {
      this.browserProxy_.removeProfileShortcut();
    }
  }
});
