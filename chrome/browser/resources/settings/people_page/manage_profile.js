// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-profile' is the settings subpage containing controls to
 * edit a profile's name, icon, and desktop shortcut.
 */
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/paper-styles/shadow.js';
import '../settings_shared_css.m.js';

import {AvatarIcon} from 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {RouteObserverBehavior, Router} from '../router.m.js';

import {ManageProfileBrowserProxy, ManageProfileBrowserProxyImpl, ProfileShortcutStatus} from './manage_profile_browser_proxy.js';
import {SyncStatus} from './sync_browser_proxy.m.js';

Polymer({
  is: 'settings-manage-profile',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior, RouteObserverBehavior],

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
      value() {
        return [];
      },
    },

    /**
     * The current sync status.
     * @type {?SyncStatus}
     */
    syncStatus: Object,

    /**
     * True if the profile shortcuts feature is enabled.
     */
    isProfileShortcutSettingVisible_: Boolean,

    /**
     * True if the customize themes feature is enabled.
     * @private
     */
    isCustomizeThemesVisible_: {
      type: Boolean,
      value: () => loadTimeData.getBoolean('profileThemeSelectorEnabled')
    },

    /**
     * TODO(dpapad): Move this back to the HTML file when the Polymer2 version
     * of the code is deleted. Because of "\" being a special character in a JS
     * string, can't satisfy both Polymer2 and Polymer3 at the same time from
     * the HTML file.
     * @private
     */
    pattern_: {
      type: String,
      value: '.*\\S.*',
    },
  },

  /** @private {?ManageProfileBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = ManageProfileBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    const setIcons = icons => {
      this.availableIcons = icons;
    };

    this.addWebUIListener('available-icons-changed', setIcons);
    this.browserProxy_.getAvailableIcons().then(setIcons);
  },

  /** @protected */
  currentRouteChanged() {
    if (Router.getInstance().getCurrentRoute() === routes.MANAGE_PROFILE) {
      if (this.profileName) {
        this.$.name.value = this.profileName;
      }
      if (loadTimeData.getBoolean('profileShortcutsEnabled')) {
        this.browserProxy_.getProfileShortcutStatus().then(status => {
          if (status ===
              ProfileShortcutStatus.PROFILE_SHORTCUT_SETTING_HIDDEN) {
            this.isProfileShortcutSettingVisible_ = false;
            return;
          }

          this.isProfileShortcutSettingVisible_ = true;
          this.hasProfileShortcut_ =
              status === ProfileShortcutStatus.PROFILE_SHORTCUT_FOUND;
        });
      }
    }
  },

  /**
   * Handler for when the profile name field is changed, then blurred.
   * @param {!Event} event
   * @private
   */
  onProfileNameChanged_(event) {
    if (event.target.invalid) {
      return;
    }

    this.browserProxy_.setProfileName(event.target.value);
  },

  /**
   * Handler for profile name keydowns.
   * @param {!Event} event
   * @private
   */
  onProfileNameKeydown_(event) {
    if (event.key === 'Escape') {
      event.target.value = this.profileName;
      event.target.blur();
    }
  },

  /**
   * Handler for when the profile avatar is changed by the user.
   * @private
   */
  profileAvatarChanged_() {
    if (this.profileAvatar_.isGaiaAvatar) {
      this.browserProxy_.setProfileIconToGaiaAvatar();
    } else {
      this.browserProxy_.setProfileIconToDefaultAvatar(this.profileAvatar_.url);
    }
  },

  /**
   * @param {?SyncStatus} syncStatus
   * @return {boolean} Whether the profile name field is disabled.
   * @private
   */
  isProfileNameDisabled_(syncStatus) {
    return !!syncStatus.supervisedUser && !syncStatus.childUser;
  },

  /**
   * Handler for when the profile shortcut toggle is changed.
   * @param {!Event} event
   * @private
   */
  onHasProfileShortcutChange_(event) {
    if (this.hasProfileShortcut_) {
      this.browserProxy_.addProfileShortcut();
    } else {
      this.browserProxy_.removeProfileShortcut();
    }
  }
});
