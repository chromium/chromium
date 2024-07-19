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
import './pages/cpu_usage_chart.js';
import './pages/thermal_chart.js';
import './settings/settings_dialog.js';

import {sendWithPromise} from '//resources/js/cr.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {PagePath} from './constants.js';
import {DataManager} from './data_manager.js';
import type {HealthdInternalsFeatureFlagResult} from './externs.js';
import type {HealthdInternalsBatteryChartElement} from './pages/battery_chart.js';
import type {HealthdInternalsCpuFrequencyChartElement} from './pages/cpu_frequency_chart.js';
import type {HealthdInternalsCpuUsageChartElement} from './pages/cpu_usage_chart.js';
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
    cpuUsageChart: HealthdInternalsCpuUsageChartElement,
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

    this.dataManager = new DataManager(
        this.$.settingsDialog.getDataRetentionDuration(),
        this.$.telemetryPage,
        this.$.batteryChart,
        this.$.cpuFrequencyChart,
        this.$.cpuUsageChart,
        this.$.thermalChart,
    );

    this.$.settingsDialog.addEventListener('polling-cycle-updated', () => {
      this.setupFetchDataRequests();
    });

    this.$.settingsDialog.addEventListener('data-retention-updated', () => {
      this.updateDataRetentionDuration();
    });

    sendWithPromise('getHealthdInternalsFeatureFlag')
        .then((data: HealthdInternalsFeatureFlagResult) => {
          if (!data.tabsDisplayed) {
            this.currentPath = PagePath.NONE;
            return;
          }

          this.pageList = [
            {name: 'Telemetry', path: PagePath.TELEMETRY},
            {name: 'Battery Chart', path: PagePath.BATTERY},
            {name: 'CPU Frequency Chart', path: PagePath.CPU_FREQUENCY},
            {name: 'CPU Usage Chart', path: PagePath.CPU_USAGE},
            {name: 'Thermal Chart', path: PagePath.THERMAL},
          ];

          // `currentPath` will be set when chrome://healthd-internals is open.
          // Update the selected index to render the page after `pageList` is
          // set.
          this.updateSelectedIndex(this.currentPath);
          this.handleVisibilityChanged(this.currentPath, true);
          this.setupFetchDataRequests();
          this.updateDataRetentionDuration();
        });
  }

  // Init in `connectedCallback`.
  private dataManager: DataManager;

  // The content pages for chrome://healthd-internals. It is also used for
  // rendering the tabs in the sidebar menu.
  // It will be empty if the feature flag (HealthdInternalsTabs) is disabled.
  private pageList: Page[] = [];
  // This current path updated by `iron-location`.
  private currentPath: string = PagePath.NONE;
  // The selected index updated by `cr-menu-selector`. If `pageList` is empty,
  // this index will always be -1 and no page will be displayed.
  private selectedIndex: number = -1;

  // Return true if the menu tabs are not displayed.
  private areTabsHidden(): boolean {
    return !this.pageList.length;
  }

  // Handle path changes caused by popstate events (back/forward navigation).
  private currentPathChanged(newPath: string, oldPath: string) {
    if (this.areTabsHidden()) {
      return;
    }
    this.updateSelectedIndex(newPath);
    this.handleVisibilityChanged(newPath, true);
    this.handleVisibilityChanged(oldPath, false);
  }

  // Handle selected index changes caused by clicking on navigation items.
  private selectedIndexChanged() {
    if (this.areTabsHidden()) {
      return;
    }
    if (this.selectedIndex >= 0 && this.selectedIndex < this.pageList.length) {
      this.currentPath = this.pageList[this.selectedIndex].path;
    }
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
    } else if (pagePath === PagePath.CPU_USAGE) {
      this.$.cpuUsageChart.updateVisibility(isVisible);
    } else if (pagePath === PagePath.THERMAL) {
      this.$.thermalChart.updateVisibility(isVisible);
    }
  }

  // Set up periodic fetch data requests to the backend to get required info.
  private setupFetchDataRequests() {
    if (this.areTabsHidden()) {
      console.warn(
          'Data fetching requests are ignored when tabs are not displayed.');
      return;
    }

    this.dataManager.setupFetchDataRequests(
        this.$.settingsDialog.getHealthdDataPollingCycle() * 1000);
  }

  private openSettingsDialog() {
    this.$.settingsDialog.openSettingsDialog();
  }

  private updateDataRetentionDuration() {
    if (!this.pageList.length) {
      console.warn(
          'Data retention duration is ignored when tabs are not displayed.');
      return;
    }

    const duration: number = this.$.settingsDialog.getDataRetentionDuration();
    this.dataManager.updateDataRetentionDuration(duration);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-app': HealthdInternalsAppElement;
  }
}

customElements.define(
    HealthdInternalsAppElement.is, HealthdInternalsAppElement);
