// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './battery_page.js';
import './memory_page.js';
import './performance_page.js';
import './speed_page.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PerformanceBrowserProxyImpl} from '../performance_page/performance_browser_proxy.js';
import {routes} from '../route.js';
import {RouteObserverMixinLit} from '../router.js';
import type {Route} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixinLit} from '../settings_page/searchable_view_container_mixin_lit.js';

import {getCss} from './performance_page_index.css.js';
import {getHtml} from './performance_page_index.html.js';


export interface SettingsPerformancePageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsPerformancePageIndexElementBase = SearchableViewContainerMixinLit(
    RouteObserverMixinLit(WebUiListenerMixinLit(CrLitElement)));

export class SettingsPerformancePageIndexElement extends
    SettingsPerformancePageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-performance-page-index';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // Used to hide battery settings section if the device has no battery.
  static override get properties() {
    return {
      showBatterySettings_: {type: Boolean},
    };
  }

  protected accessor showBatterySettings_: boolean = false;

  private showDefaultViews_() {
    this.$.viewManager.switchViews(
        ['performance', 'memory', 'battery', 'speed'], 'no-animation',
        'no-animation');
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'device-has-battery-changed',
        this.onDeviceHasBatteryChanged_.bind(this));
    PerformanceBrowserProxyImpl.getInstance().getDeviceHasBattery().then(
        this.onDeviceHasBatteryChanged_.bind(this));
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.PERFORMANCE:
          this.showDefaultViews_();
          break;
        case routes.BASIC:
          // Switch back to the default views in case they are part of search
          // results.
          this.showDefaultViews_();
          break;
        default:
          // Nothing to do. Other parent elements are responsible for updating
          // the displayed contents.
          break;
      }
    });
  }

  private onDeviceHasBatteryChanged_(deviceHasBattery: boolean) {
    this.showBatterySettings_ = deviceHasBattery;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-performance-page-index': SettingsPerformancePageIndexElement;
  }
}

export {SettingsPerformancePageIndexElement as PerformancePageIndexElement};

customElements.define(
    SettingsPerformancePageIndexElement.is,
    SettingsPerformancePageIndexElement);
