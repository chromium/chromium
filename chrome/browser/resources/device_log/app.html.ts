// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DeviceLogAppElement} from './app.js';

export function getHtml(this: DeviceLogAppElement) {
  return html`<!--_html_template_start_-->
  <div id="header">
    <span>$i18n{autoRefreshText}</span>
  </div>
  <div id="logOptionsContainer">
    <button id="logRefresh">$i18n{logRefreshText}</button>
    <button id="logClear">$i18n{logClearText}</button>
    <span>$i18n{logLevelLabel}</span>
    <select id="logLevelSelect">
      <option value="Error">$i18n{logLevelErrorText}</option>
      <option value="User">$i18n{logLevelUserText}</option>
      <option value="Event">$i18n{logLevelEventText}</option>
      <option value="Debug">$i18n{logLevelDebugText}</option>
    </select>
    <label>
      <input id="logFileinfo" type="checkbox">
      <span>$i18n{logLevelFileinfoText}</span>
    </label>
    <label>
      <input id="logTimedetail" type="checkbox">
      <span>$i18n{logLevelTimeDetailText}</span>
    </label>
  </div>

  <div id="logCheckboxContainer">
    <button id="logClearTypes">$i18n{logClearTypesText}</button>
    <label>
      <input id="logTypebluetooth" type="checkbox">
      <span>$i18n{logTypeBluetoothText}</span>
    </label>
    <label>
      <input id="logTypecamera" type="checkbox">
      <span>$i18n{logTypeCameraText}</span>
    </label>
    <label>
      <input id="logTypedisplay" type="checkbox">
      <span>$i18n{logTypeDisplayText}</span>
    </label>
    <label>
      <input id="logTypeextensions" type="checkbox">
      <span>$i18n{logTypeExtensionsText}</span>
    </label>
    <label>
      <input id="logTypefido" type="checkbox">
      <span>$i18n{logTypeFidoText}</span>
    </label>
    <label>
      <input id="logTypefirmware" type="checkbox">
      <span>$i18n{logTypeFirmwareText}</span>
    </label>
    <label>
      <input id="logTypegeolocation" type="checkbox">
      <span>$i18n{logTypeGeolocationText}</span>
    </label>
    <label>
      <input id="logTypeHid" type="checkbox">
      <span>$i18n{logTypeHidText}</span>
    </label>
    <label>
      <input id="logTypelogin" type="checkbox">
      <span>$i18n{logTypeLoginText}</span>
    </label>
    <label>
      <input id="logTypenetwork" type="checkbox">
      <span>$i18n{logTypeNetworkText}</span>
    </label>
    <label>
      <input id="logTypepower" type="checkbox">
      <span>$i18n{logTypePowerText}</span>
    </label>
    <label>
      <input id="logTypeprinter" type="checkbox">
      <span>$i18n{logTypePrinterText}</span>
    </label>
    <label>
      <input id="logTypeserial" type="checkbox">
      <span>$i18n{logTypeSerialText}</span>
    </label>
    <label>
      <input id="logTypeusb" type="checkbox">
      <span>$i18n{logTypeUsbText}</span>
    </label>
  </div>
  <div id="typeHint">
    <span>$i18n{autoSelectTypes}</span>
  </div>
  <div id="logContainer"></div>
  <!--_html_template_end_-->`;
}
