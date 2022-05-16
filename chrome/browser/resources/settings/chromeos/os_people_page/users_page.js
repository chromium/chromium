// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-page' is the settings page for managing user accounts on
 * the device.
 */

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/js/action_link.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';
import './user_list.js';
import './users_add_user_dialog.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

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
    return html`{__html_template__}`;
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
       * @type {!Set<!chromeos.settings.mojom.Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          chromeos.settings.mojom.Setting.kGuestBrowsingV2,
          chromeos.settings.mojom.Setting.kShowUsernamesAndPhotosAtSignInV2,
          chromeos.settings.mojom.Setting.kRestrictSignInV2,
          chromeos.settings.mojom.Setting.kAddToUserAllowlistV2,
          chromeos.settings.mojom.Setting.kRemoveFromUserAllowlistV2,
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
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId !==
        chromeos.settings.mojom.Setting.kRemoveFromUserAllowlistV2) {
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
