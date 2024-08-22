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
import './pages/generic_chart.js';
import './pages/process.js';
import './pages/telemetry.js';
import './settings/settings_dialog.js';

import {sendWithPromise} from '//resources/js/cr.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {PagePath} from './constants.js';
import {DataManager} from './data_manager.js';
import type {HealthdInternalsFeatureFlagResult} from './externs.js';
import type {HealthdInternalsGenericChartElement} from './pages/generic_chart.js';
import type {HealthdInternalsProcessElement} from './pages/process.js';
import type {HealthdInternalsTelemetryElement} from './pages/telemetry.js';
import {HealthdInternalsPage} from './pages/utils/page_interface.js';
import type {HealthdInternalsSettingsDialogElement} from './settings/settings_dialog.js';

// Interface of pages in chrome://healthd-internals.
interface Page {
  name: string;
  path: PagePath;
  obj: HealthdInternalsPage;
}

export interface HealthdInternalsAppElement {
  $: {
    telemetryPage: HealthdInternalsTelemetryElement,
    processPage: HealthdInternalsProcessElement,
    batteryChart: HealthdInternalsGenericChartElement,
    cpuFrequencyChart: HealthdInternalsGenericChartElement,
    cpuUsageChart: HealthdInternalsGenericChartElement,
    memoryChart: HealthdInternalsGenericChartElement,
    thermalChart: HealthdInternalsGenericChartElement,
    settingsDialog: HealthdInternalsSettingsDialogElement,
    appContainer: HTMLElement,
    sidebar: HTMLElement,
    sidebarToggleButton: HTMLElement,
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

    this.initLineChartPages();
    this.dataManager = new DataManager(
        this.$.settingsDialog.getDataRetentionDuration(), this.$.telemetryPage,
        {
          battery: this.$.batteryChart,
          cpuFrequency: this.$.cpuFrequencyChart,
          cpuUsage: this.$.cpuUsageChart,
          memory: this.$.memoryChart,
          thermal: this.$.thermalChart,
        });

    this.$.settingsDialog.addEventListener('ui-update-interval-updated', () => {
      this.updateUiUpdateInterval();
    });

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
            {
              name: 'Telemetry',
              path: PagePath.TELEMETRY,
              obj: this.$.telemetryPage,
            },
            {
              name: 'Process Viewer',
              path: PagePath.PROCESS,
              obj: this.$.processPage,
            },
            {
              name: 'Battery Chart',
              path: PagePath.BATTERY,
              obj: this.$.batteryChart,
            },
            {
              name: 'CPU Frequency Chart',
              path: PagePath.CPU_FREQUENCY,
              obj: this.$.cpuFrequencyChart,
            },
            {
              name: 'CPU Usage Chart',
              path: PagePath.CPU_USAGE,
              obj: this.$.cpuUsageChart,
            },
            {
              name: 'Memory Chart',
              path: PagePath.MEMORY,
              obj: this.$.memoryChart,
            },
            {
              name: 'Thermal Chart',
              path: PagePath.THERMAL,
              obj: this.$.thermalChart,
            },
          ];

          // `currentPath` will be set when chrome://healthd-internals is open.
          // Update the selected index to render the page after `pageList` is
          // set.
          this.updateSelectedIndex(this.currentPath);
          this.updateUiUpdateInterval();
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

  // Init all line chart pages.
  private initLineChartPages() {
    this.$.batteryChart.setupChartHeader('Battery');
    this.$.batteryChart.initCanvasDrawer([''], 1);

    this.$.cpuFrequencyChart.setupChartHeader('CPU Frequency');
    this.$.cpuFrequencyChart.initCanvasDrawer(['kHz', 'mHz', 'GHz'], 1000);

    this.$.cpuUsageChart.setupChartHeader('CPU Usage');
    this.$.cpuUsageChart.initCanvasDrawer(['%'], 1);
    this.$.cpuUsageChart.setChartMaxValue(100);

    this.$.memoryChart.setupChartHeader('Memory');
    this.$.memoryChart.initCanvasDrawer(['KiB', 'MiB', 'GiB'], 1024);

    this.$.thermalChart.setupChartHeader('Thermal');
    this.$.thermalChart.initCanvasDrawer(['C'], 1);
  }

  // Handle path changes caused by popstate events (back/forward navigation).
  private currentPathChanged(newValue: string) {
    if (this.areTabsHidden()) {
      return;
    }
    this.updateSelectedIndex(newValue);
  }

  // Handle selected index changes caused by clicking on navigation items.
  private selectedIndexChanged(newIndex: number, oldIndex: number) {
    if (this.areTabsHidden()) {
      return;
    }
    if (newIndex >= 0 && newIndex < this.pageList.length) {
      this.currentPath = this.pageList[newIndex].path;
    }

    this.handleVisibilityChanged(newIndex, true);
    this.handleVisibilityChanged(oldIndex, false);
  }

  // Update the selected index when `pageList` is not empty. Redirect to
  // telemtry page when the path is not in `pageList`.
  private updateSelectedIndex(newPath: string) {
    const pageIndex = Math.max(
        0, this.pageList.findIndex((page: Page) => page.path === newPath));
    this.selectedIndex = pageIndex;
  }

  // Update the visibility of line chart pages to render the visible page only.
  private handleVisibilityChanged(pageIndex: number, isVisible: boolean) {
    if (pageIndex >= 0 && pageIndex < this.pageList.length) {
      this.pageList[pageIndex].obj.updateVisibility(isVisible);
    }
  }

  private updateUiUpdateInterval() {
    const interval: number = this.$.settingsDialog.getUiUpdateInterval();
    for (const page of this.pageList) {
      page.obj.updateUiUpdateInterval(interval);
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
        this.$.settingsDialog.getHealthdDataPollingCycle());
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

  private toggleSidebar() {
    this.$.sidebar.hidden = !this.$.sidebar.hidden;
    this.$.sidebarToggleButton.innerText = this.$.sidebar.hidden ? '>' : '<';
    this.$.appContainer.style.setProperty(
        '--sidebar-width', this.$.sidebar.hidden ? '0px' : '220px');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-app': HealthdInternalsAppElement;
  }
}

customElements.define(
    HealthdInternalsAppElement.is, HealthdInternalsAppElement);
