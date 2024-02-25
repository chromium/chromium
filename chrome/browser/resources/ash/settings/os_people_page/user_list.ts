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

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {CrA11yAnnouncerElement, getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Router, routes} from '../router.js';

import {getTemplate} from './user_list.html.js';

declare global {
  interface HTMLElementEventMap {
    'all-managed-users-removed': CustomEvent<void>;
  }
}

const SettingsUserListElementBase =
    RouteObserverMixin(I18nMixin(CrScrollableMixin(PolymerElement)));


export class SettingsUserListElement extends SettingsUserListElementBase {
  static get is() {
    return 'settings-user-list' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Current list of allowed users.
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
       */
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  disabled: boolean;
  private users_: chrome.usersPrivate.User[];
  private usersPrivate_: typeof chrome.usersPrivate;

  constructor() {
    super();

    this.usersPrivate_ = chrome.usersPrivate;
  }

  override ready(): void {
    super.ready();

    chrome.settingsPrivate.onPrefsChanged.addListener(
        (prefs: chrome.settingsPrivate.PrefObject[]) => {
          prefs.forEach((pref: chrome.settingsPrivate.PrefObject) => {
            if (pref.key === 'cros.accounts.users') {
              this.usersPrivate_.getUsers().then(
                  (users: chrome.usersPrivate.User[]) => {
                    this.setUsers_(users);
                  });
            }
          }, this);
        });
  }

  override currentRouteChanged(): void {
    if (Router.getInstance().currentRoute === routes.ACCOUNTS) {
      this.usersPrivate_.getUsers().then(
          (users: chrome.usersPrivate.User[]) => {
            this.setUsers_(users);
          });
    }
  }

  private getUserName_(user: chrome.usersPrivate.User): string {
    return user.isOwner ? this.i18n('deviceOwnerLabel', user.name) : user.name;
  }

  private setUsers_(users: chrome.usersPrivate.User[]): void {
    this.users_ = users;
    this.users_.sort((a, b) => {
      if (a.isOwner !== b.isOwner) {
        return b.isOwner ? 1 : -1;
      } else {
        return -1;
      }
    });
    this.requestUpdateScroll();
  }

  private removeUser_(e: DomRepeatEvent<chrome.usersPrivate.User>): void {
    (getAnnouncerInstance() as CrA11yAnnouncerElement)
        .announce(this.i18n('userRemovedMessage', e.model.item.name));

    // Focus the add user button since, after this removal, the only user left
    // will be the account owner.
    if (this.users_.length === 2) {
      const event = new CustomEvent(
          'all-managed-users-removed', {bubbles: true, composed: true});
      this.dispatchEvent(event);
    }

    this.usersPrivate_.removeUser(e.model.item.email);
  }

  private shouldHideCloseButton_(disabled: boolean, isUserOwner: boolean):
      boolean {
    return disabled || isUserOwner;
  }

  private getProfilePictureUrl_(user: chrome.usersPrivate.User): string {
    return 'chrome://userimage/' + user.email + '?id=' + Date.now() +
        '&frame=0';
  }

  private shouldShowEmail_(user: chrome.usersPrivate.User): boolean {
    return !user.isChild && user.name !== user.displayEmail;
  }

  /**
   * Use this function to prevent tooltips from displaying for user names. We
   * only want to display tooltips for email addresses.
   */
  private getTooltip_(user: chrome.usersPrivate.User): string {
    return !this.shouldShowEmail_(user) ? user.displayEmail : '';
  }

  private getRemoveUserTooltip_(user: chrome.usersPrivate.User): string {
    return this.i18n('removeUserTooltip', user.name);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsUserListElement.is]: SettingsUserListElement;
  }
}

customElements.define(SettingsUserListElement.is, SettingsUserListElement);
