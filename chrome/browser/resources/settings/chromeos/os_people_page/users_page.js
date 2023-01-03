// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-page' is the settings page for managing user accounts on
 * the device.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/js/action_link.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import './user_list.js';
import './users_add_user_dialog.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {focusWithoutInk} from 'chrome://resources/ash/common/focus_without_ink_js.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {Route} from '../router.js';

import {getTemplate} from './users_page.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsUsersPageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, RouteObserverBehavior], PolymerElement);

/** @polymer */
class SettingsUsersPageElement extends SettingsUsersPageElementBase {
  static get is() {
    return 'settings-users-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
          return loadTimeData.getBoolean('isChildAccount');
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kGuestBrowsingV2,
          Setting.kShowUsernamesAndPhotosAtSignInV2,
          Setting.kRestrictSignInV2,
          Setting.kAddToUserAllowlistV2,
          Setting.kRemoveFromUserAllowlistV2,
        ]),
      },
    };
  }

  /** @override */
  constructor() {
    super();

    chrome.usersPrivate.getCurrentUser(user => {
      this.isOwner_ = user.isOwner;
    });

    chrome.usersPrivate.isUserListManaged(isUserListManaged => {
      this.isUserListManaged_ = isUserListManaged;
    });
  }

  ready() {
    super.ready();

    this.addEventListener(
        'all-managed-users-removed', this.focusAddUserButton_);
  }

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId !== Setting.kRemoveFromUserAllowlistV2) {
      // Continue with deep linking attempt.
      return true;
    }

    // Wait for element to load.
    afterNextRender(this, () => {
      const userList = this.shadowRoot.querySelector('settings-user-list');
      const removeButton = userList.$$('cr-icon-button');
      if (removeButton) {
        this.showDeepLinkElement(removeButton);
        return;
      }
      console.warn(`Element with deep link id ${settingId} not focusable.`);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.ACCOUNTS) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @param {!Event} e
   * @private
   */
  openAddUserDialog_(e) {
    e.preventDefault();
    this.$.addUserDialog.open();
  }

  /** @private */
  onAddUserDialogClose_() {
    this.focusAddUserButton_();
  }

  /**
   * @param {boolean} isOwner
   * @param {boolean} isUserListManaged
   * @private
   * @return {boolean}
   */
  isEditingDisabled_(isOwner, isUserListManaged) {
    return !isOwner || isUserListManaged;
  }

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
  }

  /** @return {boolean} */
  shouldHideModifiedByOwnerLabel_() {
    return this.isUserListManaged_ || this.isOwner_;
  }

  /** @private */
  focusAddUserButton_() {
    focusWithoutInk(
        assert(this.shadowRoot.querySelector('#add-user-button a')));
  }
}

customElements.define(SettingsUsersPageElement.is, SettingsUsersPageElement);
