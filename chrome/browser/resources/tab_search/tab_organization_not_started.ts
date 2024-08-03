// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './tab_organization_not_started_image.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tab_organization_not_started.css.js';
import {getHtml} from './tab_organization_not_started.html.js';
import type {TabSearchSyncBrowserProxy} from './tab_search_sync_browser_proxy.js';
import {TabSearchSyncBrowserProxyImpl} from './tab_search_sync_browser_proxy.js';

const TabOrganizationNotStartedElementBase =
    WebUiListenerMixinLit(CrLitElement);

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

  static override get properties() {
    return {
      showFre: {type: Boolean},
      signedIn_: {type: Boolean},
    };
  }

  showFre: boolean = false;

  private signedIn_: boolean = false;
  private syncBrowserProxy_: TabSearchSyncBrowserProxy =
      TabSearchSyncBrowserProxyImpl.getInstance();

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.syncBrowserProxy_.getSignInState().then(this.setSignedIn_.bind(this));
    this.addWebUiListener('account-info-changed', this.setSignedIn_.bind(this));
  }

  getTitle(): string {
    return loadTimeData.getString(
        this.showFre ? 'notStartedTitleFRE' : 'notStartedTitle');
  }

  private setSignedIn_(signedIn: boolean) {
    this.signedIn_ = signedIn;
  }

  protected getBody_(): string {
    if (!this.signedIn_) {
      return loadTimeData.getString('notStartedBodySignedOut');
    } else if (this.showFre) {
      return loadTimeData.getString('notStartedBodyFREHeader');
    } else {
      return loadTimeData.getString('notStartedBody');
    }
  }

  protected getActionButtonAriaLabel_(): string {
    if (!this.signedIn_) {
      return loadTimeData.getString('notStartedButtonSignedOutAriaLabel');
    } else if (this.showFre) {
      return loadTimeData.getString('notStartedButtonFREAriaLabel');
    } else {
      return loadTimeData.getString('notStartedButtonAriaLabel');
    }
  }

  protected getActionButtonText_(): string {
    if (!this.signedIn_) {
      return loadTimeData.getString('notStartedButtonSignedOut');
    } else if (this.showFre) {
      return loadTimeData.getString('notStartedButtonFRE');
    } else {
      return loadTimeData.getString('notStartedButton');
    }
  }

  protected onButtonClick_() {
    if (!this.signedIn_) {
      this.fire('sign-in-click');
    } else {
      // Start a tab organization
      this.fire('organize-tabs-click');
      chrome.metricsPrivate.recordBoolean(
          'Tab.Organization.AllEntrypoints.Clicked', true);
      chrome.metricsPrivate.recordBoolean(
          'Tab.Organization.TabSearch.Clicked', true);
    }
  }

  protected onLearnMoreClick_() {
    this.fire('learn-more-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-not-started': TabOrganizationNotStartedElement;
  }
}

customElements.define(
    TabOrganizationNotStartedElement.is, TabOrganizationNotStartedElement);
