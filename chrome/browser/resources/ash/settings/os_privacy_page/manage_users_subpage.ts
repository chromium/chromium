// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-users-subpage' is the settings page for managing user
 * accounts on the device.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/action_link.css.js';
import 'chrome://resources/js/action_link.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import '../os_people_page/user_list.js';
import '../os_people_page/add_user_dialog.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isChild, isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {SettingsUsersAddUserDialogElement} from '../os_people_page/add_user_dialog.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './manage_users_subpage.html.js';

const SettingsManageUsersSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(I18nMixin(PolymerElement)));

export interface SettingsManageUsersSubpageElement {
  $: {
    addUserDialog: SettingsUsersAddUserDialogElement,
  };
}

export class SettingsManageUsersSubpageElement extends
    SettingsManageUsersSubpageElementBase {
  static get is() {
    return 'settings-manage-users-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      isOwner_: {
        type: Boolean,
        value: true,
      },

      isUserListManaged_: {
        type: Boolean,
        value: false,
      },

      isChild_: {
        type: Boolean,
        value() {
          return isChild();
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kGuestBrowsingV2,
          Setting.kShowUsernamesAndPhotosAtSignInV2,
          Setting.kRestrictSignInV2,
          Setting.kAddToUserAllowlistV2,
          Setting.kRemoveFromUserAllowlistV2,
        ]),
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },
    };
  }

  private isOwner_: boolean;
  private isUserListManaged_: boolean;
  private isChild_: boolean;
  private isRevampWayfindingEnabled_: boolean;

  constructor() {
    super();

    chrome.usersPrivate.getCurrentUser().then(
        (user: chrome.usersPrivate.User) => {
          this.isOwner_ = user.isOwner;
        });

    chrome.usersPrivate.isUserListManaged().then(
        (isUserListManaged: boolean) => {
          this.isUserListManaged_ = isUserListManaged;
        });
  }

  override ready(): void {
    super.ready();

    this.addEventListener(
        'all-managed-users-removed', this.focusAddUserButton_);
  }

  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    if (settingId !== Setting.kRemoveFromUserAllowlistV2) {
      // Continue with deep linking attempt.
      return true;
    }

    // Wait for element to load.
    afterNextRender(this, () => {
      const userList = this.shadowRoot!.querySelector('settings-user-list');
      const removeButton =
          userList!.shadowRoot!.querySelector('cr-icon-button');
      if (removeButton) {
        this.showDeepLinkElement(removeButton);
        return;
      }
      console.warn(`Element with deep link id ${settingId} not focusable.`);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  }

  override currentRouteChanged(route: Route, _oldRoute: Route): void {
    // Does not apply to this page.
    if (route !== routes.ACCOUNTS) {
      return;
    }

    this.attemptDeepLink();
  }

  private openAddUserDialog_(e: Event): void {
    e.preventDefault();
    this.$.addUserDialog.open();
  }

  private onAddUserDialogClose_(): void {
    this.focusAddUserButton_();
  }

  private isEditingDisabled_(isOwner: boolean, isUserListManaged: boolean):
      boolean {
    return !isOwner || isUserListManaged;
  }

  private isEditingUsersEnabled_(
      isOwner: boolean, isUserListManaged: boolean, allowGuest: boolean,
      isChild: boolean): boolean {
    return isOwner && !isUserListManaged && !allowGuest && !isChild;
  }

  private shouldHideModifiedByOwnerLabel_(): boolean {
    return this.isUserListManaged_ || this.isOwner_;
  }

  private focusAddUserButton_(): void {
    focusWithoutInk(
        castExists(this.shadowRoot!.querySelector('#add-user-button a')));
  }

  private getRestrictSigninSublabel_(): string|null {
    return this.isRevampWayfindingEnabled_ ?
        this.i18n('restrictSigninDescription') :
        null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsManageUsersSubpageElement.is]: SettingsManageUsersSubpageElement;
  }
}

customElements.define(
    SettingsManageUsersSubpageElement.is, SettingsManageUsersSubpageElement);
