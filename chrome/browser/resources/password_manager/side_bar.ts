// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_nav_menu_item_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shared_style.css.js';
import './icons.html.js';

import {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CredentialsChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';
import {Page, Route, RouteObserverMixin, Router, UrlParam} from './router.js';
import {getTemplate} from './side_bar.html.js';

export interface PasswordManagerSideBarElement {
  $: {
    'menu': CrMenuSelector,
    'compromisedPasswords': HTMLElement,
  };
}

export class PasswordManagerSideBarElement extends RouteObserverMixin
(PolymerElement) {
  static get is() {
    return 'password-manager-side-bar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // The id of the currently selected page.
      selectedPage_: String,

      // The count of compromised passwords currently known to the password
      // manager.
      compromisedPasswords_: Number,
    };
  }

  private selectedPage_: Page;
  private compromisedPasswords_: number;
  private insecureCredentialsChangedListener_: CredentialsChangedListener|null =
      null;

  override connectedCallback() {
    super.connectedCallback();
    this.insecureCredentialsChangedListener_ = insecureCredentials => {
      const compromisedTypes = [
        chrome.passwordsPrivate.CompromiseType.LEAKED,
        chrome.passwordsPrivate.CompromiseType.PHISHED,
      ];
      this.compromisedPasswords_ =
          insecureCredentials
              .filter(cred => {
                return !cred.compromisedInfo!.isMuted &&
                    cred.compromisedInfo!.compromiseTypes.some(type => {
                      return compromisedTypes.includes(type);
                    });
              })
              .length;
    };

    PasswordManagerImpl.getInstance().getInsecureCredentials().then(
        this.insecureCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addInsecureCredentialsListener(
        this.insecureCredentialsChangedListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.insecureCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().removeInsecureCredentialsListener(
        this.insecureCredentialsChangedListener_);
    this.insecureCredentialsChangedListener_ = null;
  }

  override currentRouteChanged(route: Route, _: Route): void {
    this.selectedPage_ = route.page;
  }

  private onSelectorActivate_(event: CustomEvent<{selected: Page}>) {
    Router.getInstance().navigateTo(event.detail.selected);
    if (event.detail.selected === Page.CHECKUP) {
      const params = new URLSearchParams();
      params.set(UrlParam.START_CHECK, 'true');
      Router.getInstance().updateRouterParams(params);
    }
  }

  private getSelectedPage_(): string {
    switch (this.selectedPage_) {
      case Page.CHECKUP_DETAILS:
        return Page.CHECKUP;
      case Page.PASSWORD_DETAILS:
        return Page.PASSWORDS;
      default:
        return this.selectedPage_;
    }
  }

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately by <iron-selector>.
   */
  private onItemClick_(e: Event) {
    e.preventDefault();
  }

  private getCompromisedPasswordsBadge_(): string {
    if (this.compromisedPasswords_ > 99) {
      return '99+';
    }
    return String(this.compromisedPasswords_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-manager-side-bar': PasswordManagerSideBarElement;
  }
}

customElements.define(
    PasswordManagerSideBarElement.is, PasswordManagerSideBarElement);
