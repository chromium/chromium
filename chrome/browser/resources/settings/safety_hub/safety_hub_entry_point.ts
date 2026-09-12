// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../settings_page/settings_section.js';
import './safety_hub_module.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import {Router} from '../router.js';
import type {Route} from '../router.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyHubEntryPoint} from '../metrics_browser_proxy.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import type {EntryPointInfo, SafetyHubBrowserProxy} from './safety_hub_browser_proxy.js';
import {SafetyHubBrowserProxyImpl} from './safety_hub_browser_proxy.js';
import {getCss} from './safety_hub_entry_point.css.js';
import {getHtml} from './safety_hub_entry_point.html.js';
import type {SettingsSafetyHubModuleElement} from './safety_hub_module.js';
// clang-format on

export interface SettingsSafetyHubEntryPointElement {
  $: {
    button: HTMLElement,
    module: SettingsSafetyHubModuleElement,
  };
}

const SettingsSafetyHubEntryPointElementBase =
    SettingsViewMixinLit(I18nMixinLit(CrLitElement));

export class SettingsSafetyHubEntryPointElement extends
    SettingsSafetyHubEntryPointElementBase {
  static get is() {
    return 'settings-safety-hub-entry-point';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      hasRecommendations_: {type: Boolean},
      headerString_: {type: String},
      subheaderString_: {type: String},
    };
  }

  private accessor hasRecommendations_: boolean = false;
  protected accessor headerString_: string = '';
  protected accessor subheaderString_: string = '';
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

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

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

  protected computeButtonClass_() {
    return this.hasRecommendations_ ? 'action-button' : '';
  }

  protected computeHeaderIconColor_() {
    return this.hasRecommendations_ ? 'blue' : '';
  }

  protected onClick_() {
    if (this.hasRecommendations_) {
      this.metricsBrowserProxy_.recordSafetyHubEntryPointClicked(
          SafetyHubEntryPoint.PRIVACY_WARNING);
    } else {
      this.metricsBrowserProxy_.recordSafetyHubEntryPointClicked(
          SafetyHubEntryPoint.PRIVACY_SAFE);
    }
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
  }

  // SettingsViewMixinLit implementation.
  override getFocusConfig() {
    return new Map([
      [routes.SAFETY_HUB.path, '#button'],
    ]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-entry-point': SettingsSafetyHubEntryPointElement;
  }
}

customElements.define(
    SettingsSafetyHubEntryPointElement.is, SettingsSafetyHubEntryPointElement);
