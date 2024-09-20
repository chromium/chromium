// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './safety_hub_module.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router, RouteObserverMixin} from '../router.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyHubEntryPoint} from '../metrics_browser_proxy.js';

import type {EntryPointInfo, SafetyHubBrowserProxy} from './safety_hub_browser_proxy.js';
import {SafetyHubBrowserProxyImpl} from './safety_hub_browser_proxy.js';
import {getTemplate} from './safety_hub_entry_point.html.js';
import type {SettingsSafetyHubModuleElement} from './safety_hub_module.js';
// clang-format on

export interface SettingsSafetyHubEntryPointElement {
  $: {
    button: HTMLElement,
    module: SettingsSafetyHubModuleElement,
  };
}

const SettingsSafetyHubEntryPointElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement));

export class SettingsSafetyHubEntryPointElement extends
    SettingsSafetyHubEntryPointElementBase {
  static get is() {
    return 'settings-safety-hub-entry-point';
  }

  static get template() {
    return getTemplate();
  }


  static get properties() {
    return {
      buttonClass_: {
        type: Boolean,
        computed: 'computeButtonClass_(hasRecommendations_)',
      },

      hasRecommendations_: {
        type: Boolean,
        value: false,
      },

      headerString_: String,

      subheaderString_: String,

      headerIconColor_: {
        type: String,
        computed: 'computeHeaderIconColor_(hasRecommendations_)',
      },
    };
  }

  private buttonClass_: string;
  private hasRecommendations_: boolean;
  private headerString_: string;
  private subheaderString_: string;
  private headerIconColor_: string;
  private safetyHubBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    this.safetyHubBrowserProxy_.getSafetyHubEntryPointData().then(
        (entryPoint: EntryPointInfo) => {
          this.hasRecommendations_ = entryPoint.hasRecommendations;
          this.headerString_ = entryPoint.header;
          this.subheaderString_ = entryPoint.subheader;
        });
    // This should be called after the data for modules are retrieved so that
    // currentRouteChanged is called afterwards.
    super.connectedCallback();
  }

  override currentRouteChanged() {
    if (Router.getInstance().getCurrentRoute() !== routes.PRIVACY) {
      return;
    }
    // Only record the metrics when the user navigates to the privacy page
    // that shows the entry point.
    if (this.hasRecommendations_) {
      this.metricsBrowserProxy_.recordSafetyHubEntryPointShown(
          SafetyHubEntryPoint.PRIVACY_WARNING);
    } else {
      this.metricsBrowserProxy_.recordSafetyHubEntryPointShown(
          SafetyHubEntryPoint.PRIVACY_SAFE);
    }
  }

  private computeButtonClass_() {
    return this.hasRecommendations_ ? 'action-button' : '';
  }

  private computeHeaderIconColor_() {
    return this.hasRecommendations_ ? 'blue' : '';
  }

  private onClick_() {
    if (this.hasRecommendations_) {
      this.metricsBrowserProxy_.recordSafetyHubEntryPointClicked(
          SafetyHubEntryPoint.PRIVACY_WARNING);
    } else {
      this.metricsBrowserProxy_.recordSafetyHubEntryPointClicked(
          SafetyHubEntryPoint.PRIVACY_SAFE);
    }
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-entry-point': SettingsSafetyHubEntryPointElement;
  }
}

customElements.define(
    SettingsSafetyHubEntryPointElement.is, SettingsSafetyHubEntryPointElement);
