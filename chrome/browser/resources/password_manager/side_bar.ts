// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_nav_menu_item_style.css.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';

import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CredentialsChangedListener} from './password_manager_proxy.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import type {Route} from './router.js';
import {Page, RouteObserverMixin, Router, UrlParam} from './router.js';
import {getTemplate} from './side_bar.html.js';

/**
 * Represents different referrers when navigating to the Password Check page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PasswordCheckReferrer in enums.xml and
 * password_check_referrer.h.
 */
enum PasswordCheckReferrer {
  SAFETY_CHECK = 0,            // Web UI, recorded in JavaScript.
  PASSWORD_SETTINGS = 1,       // Web UI, recorded in JavaScript.
  PHISH_GUARD_DIALOG = 2,      // Native UI, recorded in C++.
  PASSWORD_BREACH_DIALOG = 3,  // Native UI, recorded in C++.
  // Must be last.
  COUNT = 4,
}

export interface PasswordManagerSideBarElement {
  $: {
    menu: CrMenuSelector,
    compromisedPasswords: HTMLElement,
    settings: HTMLElement,
  };
}

const PASSWORD_MANAGER_SETTINGS_MENU_ITEM_ELEMENT_ID =
    'PasswordManagerUI::kSettingsMenuItemElementId';

const PasswordManagerSideBarElementBase =
    HelpBubbleMixin(RouteObserverMixin(PolymerElement));

export class PasswordManagerSideBarElement extends
    PasswordManagerSideBarElementBase {
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
      this.registerHelpBubble(
          PASSWORD_MANAGER_SETTINGS_MENU_ITEM_ELEMENT_ID, this.$.settings);
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
      chrome.metricsPrivate.recordEnumerationValue(
          'PasswordManager.BulkCheck.PasswordCheckReferrer',
          PasswordCheckReferrer.PASSWORD_SETTINGS, PasswordCheckReferrer.COUNT);
    }
    this.dispatchEvent(
        new CustomEvent('close-drawer', {bubbles: true, composed: true}));
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
   * accessibility purposes, taps are handled separately.
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
