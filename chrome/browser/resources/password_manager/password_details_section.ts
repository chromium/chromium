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

  override currentRouteChanged(route: Route, _: Route): void {
    if (route.page !== Page.PASSWORD_DETAILS) {
      this.selectedGroup_ = undefined;
      return;
    }

    const group = route.details as chrome.passwordsPrivate.CredentialGroup;
    if (group && group.name) {
      this.selectedGroup_ = group;
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
    const ids = selectedGroup!.entries.map(entry => entry.id);
    PasswordManagerImpl.getInstance()
        .requestCredentialsDetails(ids)
        .then(entries => {
          selectedGroup!.entries = entries;
          this.selectedGroup_ = selectedGroup;
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
