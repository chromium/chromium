// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '../strings.m.js';
import './auto_tab_groups_not_started_image.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {TabOrganizationModelStrategy} from '../tab_search.mojom-webui.js';
import type {TabSearchSyncBrowserProxy} from '../tab_search_sync_browser_proxy.js';
import {TabSearchSyncBrowserProxyImpl} from '../tab_search_sync_browser_proxy.js';

import {getCss} from './auto_tab_groups_not_started.css.js';
import {getHtml} from './auto_tab_groups_not_started.html.js';

const AutoTabGroupsNotStartedElementBase = WebUiListenerMixinLit(CrLitElement);

export interface AutoTabGroupsNotStartedElement {
  $: {
    header: HTMLElement,
  };
}

// Not started state for the auto tab groups UI.
export class AutoTabGroupsNotStartedElement extends
    AutoTabGroupsNotStartedElementBase {
  static get is() {
    return 'auto-tab-groups-not-started';
  }

  static override get properties() {
    return {
      showFre: {type: Boolean},
      modelStrategy: {type: Number},
      signedIn_: {type: Boolean},
      tabOrganizationModelStrategyEnabled_: {type: Boolean},
    };
  }

  showFre: boolean = false;
  modelStrategy: TabOrganizationModelStrategy =
      TabOrganizationModelStrategy.kTopic;

  protected tabOrganizationModelStrategyEnabled_: boolean =
      loadTimeData.getBoolean('tabOrganizationModelStrategyEnabled');
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

  protected onModelStrategyChange_(
      e: CustomEvent<{value: TabOrganizationModelStrategy}>) {
    const modelStrategy = e.detail.value;
    if (Number(modelStrategy) !== Number(this.modelStrategy)) {
      this.fire('model-strategy-change', {value: modelStrategy});
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'auto-tab-groups-not-started': AutoTabGroupsNotStartedElement;
  }
}

customElements.define(
    AutoTabGroupsNotStartedElement.is, AutoTabGroupsNotStartedElement);
