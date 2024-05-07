// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared.css.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
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
  static get is(): 'overview-card' {
    return 'overview-card' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      systemInfo: {
        type: Object,
      },

      deviceInfo: {
        type: String,
        value: '',
        computed: 'getDeviceInfo(systemInfo.versionInfo.fullVersionString,' +
            'systemInfo.boardName)',
      },

    };
  }

  private systemInfo: SystemInfo;
  private deviceInfo: string;
  private systemDataProvider: SystemDataProviderInterface =
      getSystemDataProvider();

  constructor() {
    super();
    this.fetchSystemInfo();
  }

  private fetchSystemInfo(): void {
    this.systemDataProvider.getSystemInfo().then(
        (result: {systemInfo: SystemInfo}) => {
          this.onSystemInfoReceived(result.systemInfo);
        });
  }

  private onSystemInfoReceived(systemInfo: SystemInfo): void {
    this.systemInfo = systemInfo;
  }

  private getDeviceInfo(): string {
    const marketingNameValid = !this.shouldHideMarketingName();
    const boardName = this.systemInfo.boardName;
    const version = this.systemInfo.versionInfo.fullVersionString;

    if (!boardName && !marketingNameValid) {
      const versionInfo = loadTimeData.getStringF('versionInfo', version);
      // Capitalize "v" in "version" if board and marketing name are missing.
      return versionInfo[0].toUpperCase() + versionInfo.slice(1);
    }

    const deviceInfo = this.systemInfo.boardName ?
        loadTimeData.getStringF(
            'boardAndVersionInfo', this.systemInfo.boardName, version) :
        loadTimeData.getStringF('versionInfo', version);
    return marketingNameValid ? `(${deviceInfo})` : deviceInfo;
  }

  protected shouldHideMarketingName(): boolean {
    return this.systemInfo.marketingName === 'TBD' ||
        this.systemInfo.marketingName === '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OverviewCardElement.is]: OverviewCardElement;
  }
}

customElements.define(OverviewCardElement.is, OverviewCardElement);
