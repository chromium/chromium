// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-user-list' shows a list of users whitelisted on this Chrome OS
 * device.
 *
 * Example:
 *
 *    <settings-user-list prefs="{{prefs}}">
 *    </settings-user-list>
 */
Polymer({
  is: 'settings-user-list',

  behaviors: [
    CrScrollableBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Current list of whitelisted users.
     * @private {!Array<!chrome.usersPrivate.User>}
     */
    users_: {
      type: Array,
      value: function() {
        return [];
      },
      notify: true
    },

    /**
     * Whether the user list is disabled, i.e. that no modifications can be
     * made.
     * @type {boolean}
     */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    }
  },

  /** @override */
  ready: function() {
    chrome.settingsPrivate.onPrefsChanged.addListener(prefs => {
      prefs.forEach(function(pref) {
        if (pref.key == 'cros.accounts.users') {
          chrome.usersPrivate.getWhitelistedUsers(users => {
            this.setUsers_(users);
          });
        }
      }, this);
    });
  },

  /** @protected */
  currentRouteChanged: function() {
    if (settings.getCurrentRoute() == settings.routes.ACCOUNTS) {
      chrome.usersPrivate.getWhitelistedUsers(users => {
        this.setUsers_(users);
      });
    }
  },

  /**
   * @param {!chrome.usersPrivate.User} user
   * @return {string}
   * @private
   */
  getUserName_: function(user) {
    return user.isOwner ? this.i18n('deviceOwnerLabel', user.name) : user.name;
  },

  /**
   * Helper function that sorts and sets the given list of whitelisted users.
   * @param {!Array<!chrome.usersPrivate.User>} users List of whitelisted users.
   */
  setUsers_: function(users) {
    this.users_ = users;
    this.users_.sort(function(a, b) {
      if (a.isOwner != b.isOwner) {
        return b.isOwner ? 1 : -1;
      } else {
        return -1;
      }
    });
    this.requestUpdateScroll();
  },

  /**
   * @private
   * @param {!{model: !{item: !chrome.usersPrivate.User}}} e
   */
  removeUser_: function(e) {
    chrome.usersPrivate.removeWhitelistedUser(
        e.model.item.email, /* callback */ function() {});
  },

  /** @private */
  shouldHideCloseButton_: function(disabled, isUserOwner) {
    return disabled || isUserOwner;
  },

  /**
   * @param {chrome.usersPrivate.User} user
   * @private
   */
  getProfilePictureUrl_: function(user) {
    return 'chrome://userimage/' + user.email + '?id=' + Date.now() +
        '&frame=0';
  },

  /**
   * @param {chrome.usersPrivate.User} user
   * @private
   */
  shouldShowEmail_: function(user) {
    return !user.isSupervised && user.name != user.displayEmail;
  },

  /**
   * Use this function to prevent tooltips from displaying for user names. We
   * only want to display tooltips for email addresses.
   * @param {chrome.usersPrivate.User} user
   * @private
   */
  getTooltip_: function(user) {
    return !this.shouldShowEmail_(user) ? user.displayEmail : '';
  },

  /**
   * @param {!chrome.usersPrivate.User} user
   * @return {string}
   * @private
   */
  getRemoveUserTooltip_: function(user) {
    return this.i18n('removeUserTooltip', user.name);
  },
});
