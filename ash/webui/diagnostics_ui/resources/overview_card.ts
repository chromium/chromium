// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared.css.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getSystemDataProvider} from './mojo_interface_provider.js';
import {getTemplate} from './overview_card.html.js';
import {SystemDataProviderInterface, SystemInfo} from './system_data_provider.mojom-webui.js';

/**
 * @fileoverview
 * 'overview-card' shows an overview of system information such
 * as CPU type, version, board name, and memory.
 */

export class OverviewCardElement extends PolymerElement {
  static get is() {
    return 'overview-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      systemInfo_: {
        type: Object,
      },

      deviceInfo_: {
        type: String,
        value: '',
        computed: 'getDeviceInfo_(systemInfo_.versionInfo.fullVersionString,' +
            'systemInfo_.boardName)',
      },

    };
  }

  private systemInfo_: SystemInfo;
  private deviceInfo_: string;
  private systemDataProvider_: SystemDataProviderInterface =
      getSystemDataProvider();

  constructor() {
    super();
    this.fetchSystemInfo_();
  }

  private fetchSystemInfo_(): void {
    this.systemDataProvider_.getSystemInfo().then(
        (result: {systemInfo: SystemInfo}) => {
          this.onSystemInfoReceived_(result.systemInfo);
        });
  }

  private onSystemInfoReceived_(systemInfo: SystemInfo): void {
    this.systemInfo_ = systemInfo;
  }

  private getDeviceInfo_(): string {
    const marketingNameValid = !this.shouldHideMarketingName_();
    const boardName = this.systemInfo_.boardName;
    const version = this.systemInfo_.versionInfo.fullVersionString;

    if (!boardName && !marketingNameValid) {
      const versionInfo = loadTimeData.getStringF('versionInfo', version);
      // Capitalize "v" in "version" if board and marketing name are missing.
      return versionInfo[0].toUpperCase() + versionInfo.slice(1);
    }

    const deviceInfo = this.systemInfo_.boardName ?
        loadTimeData.getStringF(
            'boardAndVersionInfo', this.systemInfo_.boardName, version) :
        loadTimeData.getStringF('versionInfo', version);
    return marketingNameValid ? `(${deviceInfo})` : deviceInfo;
  }

  protected shouldHideMarketingName_(): boolean {
    return this.systemInfo_.marketingName === 'TBD' ||
        this.systemInfo_.marketingName === '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'overview-card': OverviewCardElement;
  }
}

customElements.define(OverviewCardElement.is, OverviewCardElement);
