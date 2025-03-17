// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/ash/common/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import './healthd_internals_shared.css.js';
import './view/pages/info.js';
import './view/pages/system_trend.js';
import './view/pages/process.js';
import './view/pages/telemetry.js';
import './view/settings/settings_dialog.js';

import {sendWithPromise} from '//resources/js/cr.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {DataManager} from './model/data_manager.js';
import type {HealthdInternalsFeatureFlagResult} from './utils/externs.js';
import type {HealthdInternalsPage} from './utils/page_interface.js';
import type {HealthdInternalsInfoElement} from './view/pages/info.js';
import type {HealthdInternalsProcessElement} from './view/pages/process.js';
import type {HealthdInternalsSystemTrendElement} from './view/pages/system_trend.js';
import type {HealthdInternalsTelemetryElement} from './view/pages/telemetry.js';
import type {HealthdInternalsSettingsDialogElement} from './view/settings/settings_dialog.js';

/**
 * The enum for displayed pages in chrome://healthd-internals.
 */
export enum PagePath {
  // Only used when menu tabs are not displayed. No page should be displayed.
  NONE = '/',
  INFO = '/info',
  TELEMETRY = '/telemetry',
  PROCESS = '/process',
  SYSTEM_TREND = '/system_trend'
}

// Interface of pages in chrome://healthd-internals.
interface Page {
  name: string;
  path: PagePath;
  obj: HealthdInternalsPage;
}

export interface HealthdInternalsAppElement {
  $: {
    infoPage: HealthdInternalsInfoElement,
    telemetryPage: HealthdInternalsTelemetryElement,
    processPage: HealthdInternalsProcessElement,
    systemTrendPage: HealthdInternalsSystemTrendElement,
    settingsDialog: HealthdInternalsSettingsDialogElement,
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

    this.dataManager = new DataManager(
        this.$.settingsDialog.getDataRetentionDuration(), this.$.infoPage,
        this.$.telemetryPage, this.$.systemTrendPage.getController());

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
              name: 'Info',
              path: PagePath.INFO,
              obj: this.$.infoPage,
            },
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
              name: 'System Trend',
              path: PagePath.SYSTEM_TREND,
              obj: this.$.systemTrendPage,
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
    this.$.sidebar.classList.toggle('collapsed');
    this.$.sidebarToggleButton.innerText =
        this.$.sidebar.classList.contains('collapsed') ? '>' : '<';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-app': HealthdInternalsAppElement;
  }
}

customElements.define(
    HealthdInternalsAppElement.is, HealthdInternalsAppElement);
