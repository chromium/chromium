// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './shared_style.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './checkup_details_section.html.js';
import {CredentialsChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';
import {CheckupSubpage, Page, Route, RouteObserverMixin, Router} from './router.js';


const CheckupDetailsSectionElementBase = RouteObserverMixin(PolymerElement);

export class CheckupDetailsSectionElement extends
    CheckupDetailsSectionElementBase {
  static get is() {
    return 'checkup-details-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pageTitle_: String,

      insecurityType_: {
        type: String,
        observer: 'updateShownCredentials_',
      },

      allInsecureCredentials: {
        type: Array,
        observer: 'updateShownCredentials_',
      },

      shownInsecureCredentials: {
        type: Array,
        observer: 'onCredentialsChanged_',
      },
    };
  }

  private pageTitle_: string;
  private insecurityType_: CheckupSubpage|undefined;
  private allInsecureCredentials: chrome.passwordsPrivate.PasswordUiEntry[];
  private shownInsecureCredentials: chrome.passwordsPrivate.PasswordUiEntry[];
  private insecureCredentialsChangedListener_: CredentialsChangedListener|null =
      null;

  override connectedCallback() {
    super.connectedCallback();

    this.insecureCredentialsChangedListener_ = insecureCredentials => {
      this.allInsecureCredentials = insecureCredentials;
    };

    PasswordManagerImpl.getInstance().getInsecureCredentials().then(
        this.insecureCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addInsecureCredentialsListener(
        this.insecureCredentialsChangedListener_);
  }

  override currentRouteChanged(route: Route, _: Route): void {
    if (route.page !== Page.CHECKUP_DETAILS) {
      this.insecurityType_ = undefined;
      return;
    }
    this.insecurityType_ = route.details as unknown as CheckupSubpage;
  }

  private navigateBack_() {
    Router.getInstance().navigateTo(Page.CHECKUP);
  }

  private updateShownCredentials_() {
    if (!this.insecurityType_ || !this.allInsecureCredentials) {
      return;
    }
    this.shownInsecureCredentials = this.allInsecureCredentials.filter(cred => {
      return !cred.compromisedInfo!.isMuted &&
          cred.compromisedInfo!.compromiseTypes.some(type => {
            return this.getInsecurityType_().includes(type);
          });
    });
  }

  private async onCredentialsChanged_() {
    assert(this.insecurityType_);
    this.pageTitle_ = await PluralStringProxyImpl.getInstance().getPluralString(
        this.insecurityType_.concat('Passwords'),
        this.shownInsecureCredentials.length);
  }

  private getInsecurityType_(): chrome.passwordsPrivate.CompromiseType[] {
    assert(this.insecurityType_);
    switch (this.insecurityType_) {
      case CheckupSubpage.COMPROMISED:
        return [
          chrome.passwordsPrivate.CompromiseType.LEAKED,
          chrome.passwordsPrivate.CompromiseType.PHISHED,
        ];
      case CheckupSubpage.REUSED:
        return [chrome.passwordsPrivate.CompromiseType.REUSED];
      case CheckupSubpage.WEAK:
        return [chrome.passwordsPrivate.CompromiseType.WEAK];
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'checkup-details-section': CheckupDetailsSectionElement;
  }
}

customElements.define(
    CheckupDetailsSectionElement.is, CheckupDetailsSectionElement);
