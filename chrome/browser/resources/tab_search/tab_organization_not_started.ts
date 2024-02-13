// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './tab_organization_not_started_image.js';
import './tab_organization_shared_style.css.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_not_started.html.js';
import type {TabSearchSignInBrowserProxy} from './tab_search_sign_in_browser_proxy.js';
import {TabSearchSignInBrowserProxyImpl} from './tab_search_sign_in_browser_proxy.js';

const TabOrganizationNotStartedElementBase = WebUiListenerMixin(PolymerElement);

export interface TabOrganizationNotStartedElement {
  $: {
    header: HTMLElement,
  };
}

// Not started state for the tab organization UI.
export class TabOrganizationNotStartedElement extends
    TabOrganizationNotStartedElementBase {
  static get is() {
    return 'tab-organization-not-started';
  }

  static get properties() {
    return {
      showFre: Boolean,
      signedIn_: Boolean,
    };
  }

  showFre: boolean;

  private signedIn_: boolean = false;
  private signInBrowserProxy_: TabSearchSignInBrowserProxy =
      TabSearchSignInBrowserProxyImpl.getInstance();

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.signInBrowserProxy_.getSignInState().then(
        this.setSignedIn_.bind(this));
    this.addWebUiListener('has-account-changed', this.setSignedIn_.bind(this));
  }

  announceHeader() {
    this.$.header.textContent = '';
    this.$.header.textContent = this.getTitle_();
  }

  private setSignedIn_(signedIn: boolean) {
    this.signedIn_ = signedIn;
  }

  private getTitle_(): string {
    if (this.showFre) {
      return loadTimeData.getString('notStartedTitleFRE');
    } else {
      return loadTimeData.getString('notStartedTitle');
    }
  }

  private getBody_(): string {
    if (!this.signedIn_) {
      return loadTimeData.getString('notStartedBodySignedOut');
    } else if (this.showFre) {
      return loadTimeData.getString('notStartedBodyFRE');
    } else {
      return loadTimeData.getString('notStartedBody');
    }
  }

  private shouldShowBodyLink_(): boolean {
    return this.signedIn_ && this.showFre;
  }

  private getButtonAriaLabel_(): string {
    if (!this.signedIn_) {
      return loadTimeData.getString('notStartedButtonSignedOutAriaLabel');
    } else if (this.showFre) {
      return loadTimeData.getString('notStartedButtonFREAriaLabel');
    } else {
      return loadTimeData.getString('notStartedButtonAriaLabel');
    }
  }

  private getButtonText_(): string {
    if (!this.signedIn_) {
      return loadTimeData.getString('notStartedButtonSignedOut');
    } else if (this.showFre) {
      return loadTimeData.getString('notStartedButtonFRE');
    } else {
      return loadTimeData.getString('notStartedButton');
    }
  }

  private onButtonClick_() {
    if (!this.signedIn_) {
      this.dispatchEvent(
          new CustomEvent('sign-in-click', {bubbles: true, composed: true}));
    } else {
      // Start a tab organization
      this.dispatchEvent(new CustomEvent(
          'organize-tabs-click', {bubbles: true, composed: true}));
      chrome.metricsPrivate.recordBoolean(
          'Tab.Organization.AllEntrypoints.Clicked', true);
      chrome.metricsPrivate.recordBoolean(
          'Tab.Organization.TabSearch.Clicked', true);
    }
  }

  private onLinkClick_() {
    this.dispatchEvent(new CustomEvent('learn-more-click', {
      bubbles: true,
      composed: true,
    }));
  }

  private onLinkKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      this.onLinkClick_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-not-started': TabOrganizationNotStartedElement;
  }
}

customElements.define(
    TabOrganizationNotStartedElement.is, TabOrganizationNotStartedElement);
