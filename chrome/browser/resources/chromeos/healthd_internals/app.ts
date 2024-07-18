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
import './pages/cpu_frequency_chart.js';
import './pages/thermal_chart.js';
import './settings/settings_dialog.js';

import {sendWithPromise} from '//resources/js/cr.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {PagePath} from './constants.js';
import {HealthdApiTelemetryResult} from './externs.js';
import type {HealthdInternalsBatteryChartElement} from './pages/battery_chart.js';
import type {HealthdInternalsCpuFrequencyChartElement} from './pages/cpu_frequency_chart.js';
import type {HealthdInternalsTelemetryElement} from './pages/telemetry.js';
import type {HealthdInternalsThermalChartElement} from './pages/thermal_chart.js';
import type {HealthdInternalsSettingsDialogElement} from './settings/settings_dialog.js';

// Interface of pages in chrome://healthd-internals.
interface Page {
  name: string;
  path: PagePath;
}

export interface HealthdInternalsAppElement {
  $: {
    telemetryPage: HealthdInternalsTelemetryElement,
    batteryChart: HealthdInternalsBatteryChartElement,
    cpuFrequencyChart: HealthdInternalsCpuFrequencyChartElement,
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
        type: String,
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

    this.setupFetchDataRequests();

    this.$.settingsDialog.addEventListener('polling-cycle-updated', () => {
      this.setupFetchDataRequests();
    });
  }

  // The content pages for chrome://healthd-internals. It is also used for
  // rendering the tabs in the sidebar menu.
  private pageList: Page[] = [
    {
      name: 'Telemetry',
      path: PagePath.TELEMETRY,
    },
    {
      name: 'Battery Chart',
      path: PagePath.BATTERY,
    },
    {
      name: 'CPU Frequency Chart',
      path: PagePath.CPU_FREQUENCY,
    },
    {
      name: 'Thermal Chart',
      path: PagePath.THERMAL,
    },
  ];
  // This current path updated by `iron-location`.
  private currentPath: string;
  // The selected index updated by `cr-menu-selector`.
  private selectedIndex: number;

  // The interval ID used for cancelling the running intervals.
  private fetchDataInternalId: number|undefined = undefined;

  // Handle path changes caused by popstate events (back/forward navigation).
  private currentPathChanged(newPath: string, oldPath: string) {
    this.updateSelectedIndex(newPath);
    this.handleVisibilityChanged(newPath, true);
    this.handleVisibilityChanged(oldPath, false);
  }

  // Handle selected index changes caused by clicking on navigation items.
  private selectedIndexChanged() {
    this.currentPath = this.pageList[this.selectedIndex]!.path;
  }

  // Update the selected index when `pageList` is not empty. Redirect to
  // telemtry page when the path is not in `pageList`.
  private updateSelectedIndex(newPath: string) {
    const pageIndex = Math.max(
        0, this.pageList.findIndex((page: Page) => page.path === newPath));
    this.selectedIndex = pageIndex;
  }

  // Update the visibility of line chart pages to render the visible page only.
  private handleVisibilityChanged(pagePath: string, isVisible: boolean) {
    if (pagePath === PagePath.BATTERY) {
      this.$.batteryChart.updateVisibility(isVisible);
    } else if (pagePath === PagePath.CPU_FREQUENCY) {
      this.$.cpuFrequencyChart.updateVisibility(isVisible);
    } else if (pagePath === PagePath.THERMAL) {
      this.$.thermalChart.updateVisibility(isVisible);
    }
  }

  // Set up periodic fetch data requests to the backend to get required info.
  private setupFetchDataRequests() {
    if (this.fetchDataInternalId !== undefined) {
      clearInterval(this.fetchDataInternalId);
      this.fetchDataInternalId = undefined;
    }
    const fetchData = () => {
      sendWithPromise('getHealthdTelemetryInfo')
          .then((data: HealthdApiTelemetryResult) => {
            this.handleHealthdTelemetryInfo(data);
          });
    };
    fetchData();
    this.fetchDataInternalId = setInterval(
        fetchData, this.$.settingsDialog.getHealthdDataPollingCycle() * 1000);
  }

  private handleHealthdTelemetryInfo(data: HealthdApiTelemetryResult) {
    this.$.telemetryPage.updateTelemetryData(data);

    const timestamp: number = Date.now();
    this.$.batteryChart.updateBatteryData(data.battery, timestamp);
    this.$.cpuFrequencyChart.updateCpuData(data.cpu, timestamp);
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
