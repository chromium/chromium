// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '//resources/ash/common/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/ash/common/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import './healthd_internals_shared.css.js';
import './pages/telemetry.js';

import {sendWithPromise} from '//resources/js/cr.js';
import {CrRouter} from '//resources/js/cr_router.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {UPDATE_PERIOD} from './constants.js';
import {HealthdApiTelemetryResult} from './externs.js';

// Interface of pages in chrome://healthd-internals.
interface Page {
  name: string;
  path: string;
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
      currentPath: {type: String},
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
      path: '/telemetry',
    },
    {
      name: 'Diagnostics',
      path: '/diagnostics',
    },
    {
      name: 'Event',
      path: '/event',
    },
  ];
  private currentPath: string;
  private selectedIndex: number;

  private selectedIndexChanged() {
    this.currentPath = this.pageList[this.selectedIndex]!.path;
  }

  // Set up periodic fetch data requests to the backend to get required info.
  private startFetchDataRequests(): void {
    const fetchData = () => {
      sendWithPromise('getHealthdTelemetryInfo')
          .then((data: HealthdApiTelemetryResult) => {
            this.handleHealthdTelemetryInfo(data);
          });
    };
    fetchData();
    setInterval(fetchData, UPDATE_PERIOD);
  }

  private handleHealthdTelemetryInfo(_: HealthdApiTelemetryResult): void {
    // TODO(b/350423216): Handle the response.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-app': HealthdInternalsAppElement;
  }
}

customElements.define(
    HealthdInternalsAppElement.is, HealthdInternalsAppElement);
