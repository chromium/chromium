// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/shared/settings/prefs/prefs.js';
import '../settings_shared.css.js';
import './battery_page.js';
import './memory_page.js';
import './performance_page.js';
import './speed_page.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PerformanceBrowserProxyImpl} from '../performance_page/performance_browser_proxy.js';
import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixin} from '../settings_page/searchable_view_container_mixin.js';

import {getTemplate} from './performance_page_index.html.js';


export interface SettingsPerformancePageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsPerformancePageIndexElementBase = SearchableViewContainerMixin(
    RouteObserverMixin(WebUiListenerMixin(PolymerElement)));

export class SettingsPerformancePageIndexElement extends
    SettingsPerformancePageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-performance-page-index';
  }

  static get template() {
    return getTemplate();
  }

  // Used to hide battery settings section if the device has no battery.
  static get properties() {
    return {
      prefs: Object,

      showBatterySettings_: {
        type: Boolean,
        value: false,
      },
    };
  }

  declare prefs: {[key: string]: any};
  declare private showBatterySettings_: boolean;

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

customElements.define(
    SettingsPerformancePageIndexElement.is,
    SettingsPerformancePageIndexElement);
