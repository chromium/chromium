// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './site_favicon.js';
import './password_details_card.js';

import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_details_section.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {Page, Route, RouteObserverMixin, Router} from './router.js';

export interface PasswordDetailsSectionElement {
  $: {
    backButton: CrIconButtonElement,
    title: HTMLElement,
  };
}

const PasswordDetailsSectionElementBase = RouteObserverMixin(PolymerElement);

export class PasswordDetailsSectionElement extends
    PasswordDetailsSectionElementBase {
  static get is() {
    return 'password-details-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {selectedGroup_: Object};
  }

  private selectedGroup_: chrome.passwordsPrivate.CredentialGroup|undefined;
  private savedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.savedPasswordsListener_) {
      PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
          this.savedPasswordsListener_);
      this.savedPasswordsListener_ = null;
    }
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
    } else {
      // Navigation happened directly. Find group with matching name.
      this.assignMatchingGroup(route.details as string);
    }
  }

  private navigateBack_() {
    Router.getInstance().navigateTo(Page.PASSWORDS);
  }

  private getGroupName_(): string {
    return this.selectedGroup_ ? this.selectedGroup_!.name : '';
  }

  private async assignMatchingGroup(groupName: string) {
    const groups =
        await PasswordManagerImpl.getInstance().getCredentialGroups();
    const selectedGroup = groups.find(group => group.name === groupName);
    if (!selectedGroup) {
      this.navigateBack_();
      return;
    }
    assert(selectedGroup);
    this.updateShownCredentials(selectedGroup).catch(this.navigateBack_);
    this.startListeningForUpdates_();
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
   * Requests passwords and notes for all credentials from a group.
   */
  private updateShownCredentials(
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
    // If ids match, don't do anything.
    if (currentIds.sort().toString() === newIds.sort().toString()) {
      return;
    }
    this.updateShownCredentials(matchingGroup)
        .then(() => {
          // Use navigation to update page title if needed.
          Router.getInstance().navigateTo(
              Page.PASSWORD_DETAILS, this.selectedGroup_);
        })
        .catch(this.navigateBack_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-details-section': PasswordDetailsSectionElement;
  }
}

customElements.define(
    PasswordDetailsSectionElement.is, PasswordDetailsSectionElement);
