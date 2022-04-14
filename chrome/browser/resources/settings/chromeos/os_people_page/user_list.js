// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-user-list' shows a list of users allowed on this Chrome OS
 * device.
 *
 * Example:
 *
 *    <settings-user-list prefs="{{prefs}}">
 *    </settings-user-list>
 */
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';

import {CrScrollableBehavior} from '//resources/cr_elements/cr_scrollable_behavior.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-user-list',

  behaviors: [
    CrScrollableBehavior,
    I18nBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /**
     * Current list of allowed users.
     * @private {!Array<!chrome.usersPrivate.User>}
     */
    users_: {
      type: Array,
      value() {
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

  /** chrome.usersPrivate */
  usersPrivate_: chrome.usersPrivate,

  /** @override */
  ready() {
    chrome.settingsPrivate.onPrefsChanged.addListener(prefs => {
      prefs.forEach(function(pref) {
        if (pref.key === 'cros.accounts.users') {
          this.usersPrivate_.getUsers(
              (/** !Array<!chrome.usersPrivate.User> */ users) => {
                this.setUsers_(users);
              });
        }
      }, this);
    });
  },

  /** @protected */
  currentRouteChanged() {
    if (Router.getInstance().getCurrentRoute() === routes.ACCOUNTS) {
      this.usersPrivate_.getUsers(
          (/** !Array<!chrome.usersPrivate.User> */ users) => {
            this.setUsers_(users);
          });
    }
  },

  /**
   * @param {!chrome.usersPrivate.User} user
   * @return {string}
   * @private
   */
  getUserName_(user) {
    return user.isOwner ? this.i18n('deviceOwnerLabel', user.name) : user.name;
  },

  /**
   * Helper function that sorts and sets the given list of allowed users.
   * @param {!Array<!chrome.usersPrivate.User>} users List of allowed users.
   */
  setUsers_(users) {
    this.users_ = users;
    this.users_.sort(function(a, b) {
      if (a.isOwner !== b.isOwner) {
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
  removeUser_(e) {
    getAnnouncerInstance().announce(
        this.i18n('userRemovedMessage', e.model.item.name));

    // Focus the add user button since, after this removal, the only user left
    // will be the account owner.
    if (this.users_.length === 2) {
      this.fire('all-managed-users-removed');
    }

    this.usersPrivate_.removeUser(
        e.model.item.email, /* callback */ function() {});
  },

  /** @private */
  shouldHideCloseButton_(disabled, isUserOwner) {
    return disabled || isUserOwner;
  },

  /**
   * @param {chrome.usersPrivate.User} user
   * @private
   */
  getProfilePictureUrl_(user) {
    return 'chrome://userimage/' + user.email + '?id=' + Date.now() +
        '&frame=0';
  },

  /**
   * @param {chrome.usersPrivate.User} user
   * @private
   */
  shouldShowEmail_(user) {
    return !user.isChild && user.name !== user.displayEmail;
  },

  /**
   * Use this function to prevent tooltips from displaying for user names. We
   * only want to display tooltips for email addresses.
   * @param {chrome.usersPrivate.User} user
   * @private
   */
  getTooltip_(user) {
    return !this.shouldShowEmail_(user) ? user.displayEmail : '';
  },

  /**
   * @param {!chrome.usersPrivate.User} user
   * @return {string}
   * @private
   */
  getRemoveUserTooltip_(user) {
    return this.i18n('removeUserTooltip', user.name);
  },
});
