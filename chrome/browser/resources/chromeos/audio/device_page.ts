import {$} from 'chrome://resources/js/util.m.js';

import {DeviceData, PageCallbackRouter, PageHandlerRemote} from './audio.mojom-webui.js';
import {AudioBroker} from './audio_broker.js';
import {DeviceTable} from './device_table.js';
import {OutputPage} from './output_page.js';
import {Page} from './page.js';

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
    $('deviceTable').appendChild(this.deviceTable);
    this.setUpAudioDevices();
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
  }

  updateDeviceVolume(nodeId: number, volume: number) {
    this.deviceTable.setDeviceVolume(nodeId, volume);
  }

  updateDeviceMute(nodeId: number, isMuted: boolean) {
    this.deviceTable.setDeviceMuteState(nodeId, isMuted);
  }
}

let instance: DevicePage|null = null;
