// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';

import type {CrMenuSelectorElement} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CredentialsChangedListener} from './password_manager_proxy.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import type {Route} from './router.js';
import {Page, RouteObserverMixinLit, Router, UrlParam} from './router.js';
import {getCss} from './side_bar.css.js';
import {getHtml} from './side_bar.html.js';

/**
 * Represents different referrers when navigating to the Password Check page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
// LINT.IfChange(PasswordCheckReferrer)
enum PasswordCheckReferrer {
  SAFETY_CHECK = 0,               // Web UI, recorded in JavaScript.
  PASSWORD_SETTINGS = 1,          // Web UI, recorded in JavaScript.
  PHISH_GUARD_DIALOG = 2,         // Native UI, recorded in C++.
  PASSWORD_BREACH_DIALOG = 3,     // Native UI, recorded in C++.
  MORE_TO_FIX_BUBBLE = 4,         // Native UI, recorded in C++.
  // UNSAFE_STATE_BUBBLE = 5,     // Deprecated.
  SAFETY_CHECK_MAGIC_STACK = 6,   // Native UI, recorded in C++.
  SAFETY_CHECK_NOTIFICATION = 7,  // Native UI, recorded in C++.
  // Must be last.
  COUNT = 8,
}
// LINT.ThenChange(
//   //components/password_manager/core/browser/ui/password_check_referrer.h:PasswordCheckReferrer,
//   //tools/metrics/histograms/metadata/password/enums.xml:PasswordCheckReferrer
// )

export interface PasswordManagerSideBarElement {
  $: {
    menu: CrMenuSelectorElement,
    compromisedPasswords: HTMLElement,
  };
}

const PasswordManagerSideBarElementBase = RouteObserverMixinLit(CrLitElement);

export class PasswordManagerSideBarElement extends
    PasswordManagerSideBarElementBase {
  static get is() {
    return 'password-manager-side-bar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // The id of the currently selected page.
      selectedPage_: {type: String},

      // The count of compromised passwords currently known to the password
      // manager.
      compromisedPasswords_: {type: Number},
    };
  }

  private accessor selectedPage_: Page = Page.PASSWORDS;
  protected accessor compromisedPasswords_: number = 0;
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

  protected onSelectorIronActivate_(event: CustomEvent<{selected: Page}>) {
    Router.getInstance().navigateTo(event.detail.selected);
    if (event.detail.selected === Page.CHECKUP) {
      const params = new URLSearchParams();
      params.set(UrlParam.START_CHECK, 'true');
      Router.getInstance().updateRouterParams(params);
      chrome.metricsPrivate.recordEnumerationValue(
          'PasswordManager.BulkCheck.PasswordCheckReferrer',
          PasswordCheckReferrer.PASSWORD_SETTINGS, PasswordCheckReferrer.COUNT);
    }
    this.fire('close-drawer');
  }

  protected getSelectedPage_(): string {
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
  protected onItemClick_(e: Event) {
    e.preventDefault();
  }

  protected getCompromisedPasswordsBadge_(): string {
    if (this.compromisedPasswords_ > 99) {
      return '99+';
    }
    return String(this.compromisedPasswords_);
  }

  protected getInventoryIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:inventory' :
        'passwords-icon:checkup-old';
  }

  protected getSettingsIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:settings' :
        'passwords-icon:settings-old';
  }
}

export type SideBarElement = PasswordManagerSideBarElement;

declare global {
  interface HTMLElementTagNameMap {
    'password-manager-side-bar': PasswordManagerSideBarElement;
  }
}

customElements.define(
    PasswordManagerSideBarElement.is, PasswordManagerSideBarElement);
