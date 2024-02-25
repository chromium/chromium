// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$, getRequiredElement} from 'chrome://resources/js/util.js';

import {DeviceData} from './audio.mojom-webui.js';
import {DeviceMap} from './device_page.js';

export class DeviceTable extends HTMLTableElement {
  private tbody: HTMLTableSectionElement;
  private devices: DeviceMap;
  constructor() {
    super();
    this.devices = {};
    const clone =
        getRequiredElement<HTMLTemplateElement>('deviceTable-template')
                      .content.cloneNode(true);
    const thead = (clone as HTMLTableElement).querySelector('thead')!;
    this.appendChild(thead);
    this.tbody = this.createTBody();
    this.appendChild(this.tbody);
  }

  setDevices(deviceMap: DeviceMap) {
    this.devices = deviceMap;
    this.redrawTable();
    this.checkInactiveDevice();
    this.checkMutedDevice();
  }

  setDeviceVolume(nodeId: number, volume: number) {
    if (nodeId in this.devices) {
      (this.devices[nodeId] as DeviceData).volumeGainPercent = volume;
      const row = $<HTMLTableRowElement>(String(nodeId));
      if (row && row.cells[3]) {
        row.cells[3].textContent = String(volume) + '%';
      }
    }
  }

  setDeviceMuteState(nodeId: number, isMuted: boolean) {
    if (nodeId in this.devices) {
      (this.devices[nodeId] as DeviceData).isMuted = isMuted;
      const row = $<HTMLTableRowElement>(String(nodeId));
      if (row && row.cells[4]) {
        row.cells[4].textContent = String(isMuted);
      }
    }
    this.checkMutedDevice();
  }

  redrawTable() {
    this.removeChild(this.tbody);
    this.tbody = this.createTBody();
    this.appendChild(this.tbody);
    for (const item of Object.values(this.devices)) {
      const device = item as DeviceData;
      if (!(device.type === 'POST_MIX_LOOPBACK' ||
            device.type === 'POST_DSP_LOOPBACK')) {
        this.addRow(device);
      }
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
        document.createTextNode(String(device.volumeGainPercent) + '%'));
    newMuted.appendChild(document.createTextNode(String(device.isMuted)));
  }

  checkMutedDevice() {
    let hasMuted = false;
    for (const item of Object.values(this.devices)) {
      const device = item as DeviceData;
      if (device.isMuted) {
        hasMuted = true;
      }
    }
    if (hasMuted) {
      this.updateWarningBanner('muted');
    } else {
      getRequiredElement('warning-banner').hidden = true;
    }
  }

  checkInactiveDevice() {
    let allInactive = true;
    for (const item of Object.values(this.devices)) {
      const device = item as DeviceData;
      if (device.isActive) {
        allInactive = false;
      }
    }
    if (allInactive) {
      this.updateWarningBanner('inactive');
    } else {
      getRequiredElement('warning-banner').hidden = true;
    }
  }

  updateWarningBanner(reason: string) {
    getRequiredElement('warning-banner').hidden = false;
    if (reason === 'inactive') {
      getRequiredElement('warning-msg').innerHTML =
          'Warning: There is no detected active audio device. ' +
          'Have a device connected but cannot see it on the table or marked as active?';
    }
    if (reason === 'muted') {
      getRequiredElement('warning-msg').innerHTML =
          'Warning: One or more of your active devices is muted. ' +
          'Please try unmuting it by toggling the audio setting. ' +
          'Have unmuted the device but still see it marked as muted?';
    }
  }
}
customElements.define('device-table', DeviceTable, {extends: 'table'});
