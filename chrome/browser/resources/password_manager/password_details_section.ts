// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './site_favicon.js';
import './credential_details/password_details_card.js';
import './credential_details/passkey_details_card.js';
import './user_utils_mixin.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_details_section.html.js';
import {PasswordManagerImpl, PasswordViewPageInteractions} from './password_manager_proxy.js';
import type {Route} from './router.js';
import {Page, RouteObserverMixin, Router} from './router.js';
import {UserUtilMixin} from './user_utils_mixin.js';

export interface PasswordDetailsSectionElement {
  $: {
    backButton: CrIconButtonElement,
    title: HTMLElement,
  };
}

const PasswordDetailsSectionElementBase =
    PrefsMixin(UserUtilMixin(RouteObserverMixin(PolymerElement)));

export class PasswordDetailsSectionElement extends
    PasswordDetailsSectionElementBase {
  static get is() {
    return 'password-details-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedGroup_: {
        type: Object,
        observer: 'maybeRegisterPasswordSharingHelpBubble_',
      },
    };
  }

  private selectedGroup_: chrome.passwordsPrivate.CredentialGroup|undefined;
  private savedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;
  private passwordManagerAuthTimeoutListener_: () => void;
  private visibilityChangedListener_: () => void;

  override connectedCallback() {
    super.connectedCallback();

    this.passwordManagerAuthTimeoutListener_ = () => {
      if (Router.getInstance().currentRoute.page !== Page.PASSWORD_DETAILS) {
        return;
      }

      this.dispatchEvent(new CustomEvent('auth-timed-out', {
        bubbles: true,
        composed: true,
      }));
      this.navigateBack_();
      PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
          PasswordViewPageInteractions.TIMED_OUT_IN_VIEW_PAGE);
    };
    PasswordManagerImpl.getInstance().addPasswordManagerAuthTimeoutListener(
        this.passwordManagerAuthTimeoutListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.savedPasswordsListener_) {
      PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
          this.savedPasswordsListener_);
      this.savedPasswordsListener_ = null;
    }
    PasswordManagerImpl.getInstance().removePasswordManagerAuthTimeoutListener(
        this.passwordManagerAuthTimeoutListener_);
  }

  override currentRouteChanged(route: Route, _: Route): void {
    if (route.page !== Page.PASSWORD_DETAILS) {
      this.selectedGroup_ = undefined;
      return;
    }

    const group = route.details as chrome.passwordsPrivate.CredentialGroup;
    if (group && group.name) {
      this.selectedGroup_ = group;
      this.startListeningForUpdates_();
      setTimeout(() => {  // Async to allow page to load.
        this.$.backButton.focus();
      });
    } else {
      // Navigation happened directly. Find group with matching name.
      PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
          PasswordViewPageInteractions.CREDENTIAL_REQUESTED_BY_URL);
      this.assignMatchingGroup(route.details as string);
    }
  }

  private navigateBack_() {
    // Keep search query when navigating back.
    Router.getInstance().navigateTo(
        Page.PASSWORDS, null,
        Router.getInstance().currentRoute.queryParameters);
  }

  private async assignMatchingGroup(groupName: string) {
    const groups =
        await PasswordManagerImpl.getInstance().getCredentialGroups();
    let selectedGroup = groups.find(group => group.name === groupName);
    if (!selectedGroup) {
      // Check if any password in a group has matching domain.
      selectedGroup = groups.find(
          group => group.entries.some(
              entry => entry.affiliatedDomains?.some(
                  domain => domain.name === groupName)));
    }
    if (!selectedGroup) {
      this.navigateBack_();
      PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
          PasswordViewPageInteractions.CREDENTIAL_NOT_FOUND);
      return;
    }
    assert(selectedGroup);
    this.updateShownCredentials(selectedGroup)
        .then(this.startListeningForUpdates_.bind(this))
        .catch(this.navigateBack_);
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.CREDENTIAL_FOUND);
  }

  private startListeningForUpdates_() {
    if (this.savedPasswordsListener_) {
      return;
    }
    this.savedPasswordsListener_ = _passwordList => {
      PasswordManagerImpl.getInstance().getCredentialGroups().then(
          this.refreshGroupInfo_.bind(this));
    };
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.savedPasswordsListener_);
  }

  /*
   * Requests passwords and notes for all credentials from a group. If page
   * isn't visible the request will be postponed until tab becomes focused
   * again. This is done to prevent unnecessary authentication prompts.
   */
  private updateShownCredentials(
      group: chrome.passwordsPrivate.CredentialGroup): Promise<void> {
    if (document.visibilityState === 'visible') {
      return this.requestShownCredentials_(group);
    }
    return new Promise((resolve, reject) => {
      this.visibilityChangedListener_ = () => {
        if (document.visibilityState === 'visible') {
          document.removeEventListener(
              'visibilitychange', this.visibilityChangedListener_);
          this.requestShownCredentials_(group).then(resolve).catch(reject);
        }
      };
      document.addEventListener(
          'visibilitychange', this.visibilityChangedListener_);
    });
  }

  private requestShownCredentials_(
      group: chrome.passwordsPrivate.CredentialGroup): Promise<void> {
    const ids = group.entries.map(entry => entry.id);
    return PasswordManagerImpl.getInstance()
        .requestCredentialsDetails(ids)
        .then(entries => {
          group.entries = entries;
          this.selectedGroup_ = group;
        });
  }

  /*
   * Credentials have changed, check if shown credentials still exist:
   * if yes, navigates to its group page
   * if no, navigate back to Passwords page.
   */
  private refreshGroupInfo_(groups: chrome.passwordsPrivate.CredentialGroup[]) {
    assert(this.selectedGroup_);
    const currentIds = this.selectedGroup_.entries.map(entry => entry.id);
    let matchingGroup = groups.filter(
        group => group.entries.some(entry => currentIds.includes(entry.id)))[0];
    // If there is no group with matching id for PasswordUIEntry it means that
    // either PasswordUIEntry is deleted or updated. During update, site value
    // can't change so there should be a group with the same name.
    if (!matchingGroup) {
      matchingGroup =
          groups.filter(group => group.name === this.selectedGroup_!.name)[0];
      // If no group with matching name can be found it means that
      // PasswordUIEntry was deleted and group no longer exists.
      if (!matchingGroup) {
        this.navigateBack_();
        return;
      }
    }
    assert(matchingGroup);
    const newIds = matchingGroup.entries.map(entry => entry.id);
    const currentStores =
        this.selectedGroup_.entries.map(entry => entry.storedIn);
    const newStores = matchingGroup.entries.map(entry => entry.storedIn);
    // If ids match and stores used for entries haven't changed, don't do
    // anything.
    if (currentIds.sort().toString() === newIds.sort().toString() &&
        currentStores.sort().toString() === newStores.sort().toString()) {
      return;
    }
    this.updateShownCredentials(matchingGroup)
        .then(() => {
          // Use navigation to update page title if needed.
          Router.getInstance().navigateTo(
              Page.PASSWORD_DETAILS, this.selectedGroup_,
              Router.getInstance().currentRoute.queryParameters);
        })
        .catch(this.navigateBack_);
  }

  private maybeRegisterPasswordSharingHelpBubble_() {
    afterNextRender(this, () => {
      if (this.selectedGroup_?.entries[0]?.isPasskey) {
        return;
      }

      this.shadowRoot!.querySelector('password-details-card')
          ?.maybeRegisterSharingHelpBubble();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-details-section': PasswordDetailsSectionElement;
  }
}

customElements.define(
    PasswordDetailsSectionElement.is, PasswordDetailsSectionElement);
