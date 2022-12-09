// Copyright 2015 The Chromium Authors
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

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrScrollableBehavior, CrScrollableBehaviorInterface} from 'chrome://resources/ash/common/cr_scrollable_behavior.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Router} from '../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsUserListElementBase = mixinBehaviors(
    [CrScrollableBehavior, I18nBehavior, RouteObserverBehavior],
    PolymerElement);

/** @polymer */
class SettingsUserListElement extends SettingsUserListElementBase {
  static get is() {
    return 'settings-user-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Current list of allowed users.
       * @private {!Array<!chrome.usersPrivate.User>}
       */
      users_: {
        type: Array,
        value() {
          return [];
        },
        notify: true,
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
      },
    };
  }

  constructor() {
    super();

    /** @private */
    this.usersPrivate_ = chrome.usersPrivate;
  }

  /** @override */
  ready() {
    super.ready();

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
  }

  /** @protected */
  currentRouteChanged() {
    if (Router.getInstance().getCurrentRoute() === routes.ACCOUNTS) {
      this.usersPrivate_.getUsers(
          (/** !Array<!chrome.usersPrivate.User> */ users) => {
            this.setUsers_(users);
          });
    }
  }

  /**
   * @param {!chrome.usersPrivate.User} user
   * @return {string}
   * @private
   */
  getUserName_(user) {
    return user.isOwner ? this.i18n('deviceOwnerLabel', user.name) : user.name;
  }

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
  }

  /**
   * @private
   * @param {!{model: !{item: !chrome.usersPrivate.User}}} e
   */
  removeUser_(e) {
    /** @type {!CrA11yAnnouncerElement} */ (getAnnouncerInstance())
        .announce(this.i18n('userRemovedMessage', e.model.item.name));

    // Focus the add user button since, after this removal, the only user left
    // will be the account owner.
    if (this.users_.length === 2) {
      const event = new CustomEvent(
          'all-managed-users-removed', {bubbles: true, composed: true});
      this.dispatchEvent(event);
    }

    this.usersPrivate_.removeUser(
        e.model.item.email, /* callback */ function() {});
  }

  /** @private */
  shouldHideCloseButton_(disabled, isUserOwner) {
    return disabled || isUserOwner;
  }

  /**
   * @param {chrome.usersPrivate.User} user
   * @private
   */
  getProfilePictureUrl_(user) {
    return 'chrome://userimage/' + user.email + '?id=' + Date.now() +
        '&frame=0';
  }

  /**
   * @param {chrome.usersPrivate.User} user
   * @private
   */
  shouldShowEmail_(user) {
    return !user.isChild && user.name !== user.displayEmail;
  }

  /**
   * Use this function to prevent tooltips from displaying for user names. We
   * only want to display tooltips for email addresses.
   * @param {chrome.usersPrivate.User} user
   * @private
   */
  getTooltip_(user) {
    return !this.shouldShowEmail_(user) ? user.displayEmail : '';
  }

  /**
   * @param {!chrome.usersPrivate.User} user
   * @return {string}
   * @private
   */
  getRemoveUserTooltip_(user) {
    return this.i18n('removeUserTooltip', user.name);
  }
}

customElements.define(SettingsUserListElement.is, SettingsUserListElement);
