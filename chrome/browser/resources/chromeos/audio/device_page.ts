// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import {DeviceData, PageCallbackRouter, PageHandlerRemote} from './audio.mojom-webui.js';
import {AudioBroker} from './audio_broker.js';
import {DeviceTable} from './device_table.js';
import {InputPage} from './input_page.js';
import {OutputPage} from './output_page.js';
import {Page, PageNavigator} from './page.js';

export interface DeviceMap {
  [id: number]: DeviceData;
}

export class DevicePage extends Page {
  private deviceTable: DeviceTable;
  private router: PageCallbackRouter;
  private mojoHandler: PageHandlerRemote;
  constructor() {
    super('devices');
    this.router = AudioBroker.getInstance().callbackRouter;
    this.mojoHandler = AudioBroker.getInstance().handler;
    this.deviceTable = new DeviceTable();
    getRequiredElement('deviceTable').appendChild(this.deviceTable);
    this.setUpAudioDevices();
    this.setUpButtons();
  }

  setUpButtons() {
    getRequiredElement('banner-feedback').addEventListener('click', () => {
      PageNavigator.getInstance().showPage('feedback');
    });
    getRequiredElement('no-device-feedback').addEventListener('click', () => {
      PageNavigator.getInstance().showPage('feedback');
    });
  }
  setUpAudioDevices() {
    this.router.updateDeviceInfo.addListener(this.updateDeviceInfo.bind(this));
    this.router.updateDeviceVolume.addListener(
        this.updateDeviceVolume.bind(this));
    this.router.updateDeviceMute.addListener(this.updateDeviceMute.bind(this));
    this.mojoHandler.getAudioDeviceInfo();
  }

  static getInstance() {
    if (instance === null) {
      instance = new DevicePage();
    }
    return instance;
  }

  updateDeviceInfo(devices: DeviceMap) {
    this.deviceTable.setDevices(devices);
    OutputPage.getInstance().updateActiveOutputDevice();
    InputPage.getInstance().updateActiveInputDevice();
  }

  updateDeviceVolume(nodeId: number, volume: number) {
    this.deviceTable.setDeviceVolume(nodeId, volume);
  }

  updateDeviceMute(nodeId: number, isMuted: boolean) {
    this.deviceTable.setDeviceMuteState(nodeId, isMuted);
  }
}

let instance: DevicePage|null = null;
