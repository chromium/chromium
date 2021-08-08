// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {DeviceData} from './audio.mojom-webui.js';
import {DeviceMap} from './device_page.js';

export class DeviceTable extends HTMLTableElement {
  private tbody: HTMLTableSectionElement;
  private devices: DeviceMap;
  constructor() {
    super();
    this.devices = {};
    const clone = (<HTMLTemplateElement>$('deviceTable-template'))
                      .content.cloneNode(true);
    const thead = (<HTMLTableElement>clone).querySelector('thead');
    this.appendChild(<HTMLTableSectionElement>thead);
    this.tbody = this.createTBody();
    this.appendChild(this.tbody);
  }

  setDevices(deviceMap: DeviceMap) {
    this.devices = deviceMap;
    this.redrawTable();
  }

  setDeviceVolume(nodeId: number, volume: number) {
    if (nodeId in this.devices) {
      (this.devices[nodeId] as DeviceData).volumeGainPercent = volume;
      const row = <HTMLTableRowElement>$(String(nodeId));
      if (row && row.cells[3]) {
        row.cells[3].textContent = String(volume);
      }
    }
  }

  setDeviceMuteState(nodeId: number, isMuted: boolean) {
    if (nodeId in this.devices) {
      (this.devices[nodeId] as DeviceData).isMuted = isMuted;
      const row = <HTMLTableRowElement>$(String(nodeId));
      if (row && row.cells[4]) {
        row.cells[4].textContent = String(isMuted);
      }
    }
  }

  redrawTable() {
    this.removeChild(this.tbody);
    this.tbody = this.createTBody();
    this.appendChild(this.tbody);
    for (const device of Object.values(this.devices)) {
      this.addRow(device as DeviceData);
    }
  }

  addRow(device: DeviceData) {
    const newRow = this.tbody.insertRow(-1);
    newRow.id = String(device.id);
    const newName = newRow.insertCell(0);
    const newType = newRow.insertCell(1);
    const newActive = newRow.insertCell(2);
    const newVolume = newRow.insertCell(3);
    const newMuted = newRow.insertCell(4);
    newName.appendChild(document.createTextNode(device.displayName));
    newType.appendChild(document.createTextNode(device.type));
    newActive.appendChild(document.createTextNode(String(device.isActive)));
    newVolume.appendChild(
        document.createTextNode(String(device.volumeGainPercent)));
    newMuted.appendChild(document.createTextNode(String(device.isMuted)));
  }
}
customElements.define('device-table', DeviceTable, {extends: 'table'});
