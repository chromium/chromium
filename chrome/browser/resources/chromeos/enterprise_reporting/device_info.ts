// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './device_info.html.js';

/**
 * @fileoverview Presents the device information relevant to the enterprise
 * reporting page.
 */

/**
 * Interface for device information.
 */
interface DeviceInfoInterface {
  version: string;
  revision: string;
  clientId: string;
  directoryId: string;
  enrollmentDomain: string;
  obfuscatedCustomerId: string;
}

export class DeviceInfoElement extends PolymerElement {
  static get is() {
    return 'device-info-element' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      deviceInfoParsed: Object,
    };
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.parse();
  }

  private deviceInfoParsed: DeviceInfoInterface;

  private parse(): void {
    // Define and parse the device information.
    this.deviceInfoParsed = JSON.parse(loadTimeData.getString('deviceInfo'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DeviceInfoElement.is]: DeviceInfoElement;
  }
}

customElements.define(DeviceInfoElement.is, DeviceInfoElement);
