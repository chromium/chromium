// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/ash/common/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import './healthd_internals_shared.css.js';
import './pages/telemetry.js';
import './pages/battery_chart.js';
import './pages/thermal_chart.js';
import './settings/settings_dialog.js';

import {sendWithPromise} from '//resources/js/cr.js';
import {CrRouter} from '//resources/js/cr_router.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {PagePath, UPDATE_PERIOD} from './constants.js';
import {HealthdApiTelemetryResult} from './externs.js';
import type {HealthdInternalsBatteryChartElement} from './pages/battery_chart.js';
import type {HealthdInternalsThermalChartElement} from './pages/thermal_chart.js';
import type {HealthdInternalsSettingsDialogElement} from './settings/settings_dialog.js';

// Interface of pages in chrome://healthd-internals.
interface Page {
  name: string;
  path: PagePath;
}

export interface HealthdInternalsAppElement {
  $: {
    batteryChart: HealthdInternalsBatteryChartElement,
    thermalChart: HealthdInternalsThermalChartElement,
    settingsDialog: HealthdInternalsSettingsDialogElement,
  };
}

export class HealthdInternalsAppElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pageList: {type: Array},
      currentPath: {
        type: PagePath,
        observer: 'currentPathChanged',
      },
      selectedIndex: {
        type: Number,
        observer: 'selectedIndexChanged',
      },
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    const router = CrRouter.getInstance();
    this.updateSelectedIndex(router.getPath());
    this.startFetchDataRequests();
  }

  private updateSelectedIndex(newPath: string) {
    const pageIndex =
        Math.max(0, this.pageList.findIndex((page) => page.path === newPath));
    this.selectedIndex = pageIndex;
  }

  private pageList: Page[] = [
    {
      name: 'Telemetry',
      path: PagePath.TELEMETRY,
    },
    {
      name: 'Battery Diagram',
      path: PagePath.BATTERY,
    },
    {
      name: 'Thermal Diagram',
      path: PagePath.THERMAL,
    },
  ];
  private currentPath: PagePath;
  private selectedIndex: number;

  // Handle path changes caused by popstate events (back/forward navigation).
  private currentPathChanged(newPath: PagePath, oldPath: PagePath) {
    this.updateSelectedIndex(newPath);
    this.handleVisibilityChanged(newPath, true);
    this.handleVisibilityChanged(oldPath, false);
  }

  // Handle selected index changes caused by clicking on navigation items.
  private selectedIndexChanged() {
    this.currentPath = this.pageList[this.selectedIndex]!.path;
  }

  private handleVisibilityChanged(pagePath: PagePath, isVisible: boolean) {
    if (pagePath === PagePath.BATTERY) {
      this.$.batteryChart.updateVisibility(isVisible);
    } else if (pagePath === PagePath.THERMAL) {
      this.$.thermalChart.updateVisibility(isVisible);
    }
  }

  // Set up periodic fetch data requests to the backend to get required info.
  private startFetchDataRequests() {
    const fetchData = () => {
      sendWithPromise('getHealthdTelemetryInfo')
          .then((data: HealthdApiTelemetryResult) => {
            this.handleHealthdTelemetryInfo(data);
          });
    };
    fetchData();
    setInterval(fetchData, UPDATE_PERIOD);
  }

  private handleHealthdTelemetryInfo(data: HealthdApiTelemetryResult) {
    const timestamp: number = Date.now();
    this.$.batteryChart.updateBatteryData(data.battery, timestamp);
    this.$.thermalChart.updateThermalData(data.thermals, timestamp);
  }

  private openSettingsDialog() {
    this.$.settingsDialog.openSettingsDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-app': HealthdInternalsAppElement;
  }
}

customElements.define(
    HealthdInternalsAppElement.is, HealthdInternalsAppElement);
