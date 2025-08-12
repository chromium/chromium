// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * @fileoverview A helper object used by the both the Crostini and Plugin VM
 * sections to manage the file sharing and USB sharing.
 */

/**
 * Non-js key names are kept to match c++ style keys in prefs.
 */
export interface GuestId {
  vm_name: string;
  container_name: string;
}

export interface GuestOsSharedUsbDevice {
  guid: string;
  label: string;
  guestId?: GuestId|null;
  vendorId: string;
  productId: string;
  serialNumber: string;
  promptBeforeSharing: boolean;
}

export const ARC_VM_TYPE = 'arcvm';
export const BRUSCHETTA_TYPE = 'bruschetta';
export const CROSTINI_TYPE = 'crostini';
export const PLUGIN_VM_TYPE = 'pluginVm';
export const TERMINA_VM_TYPE = 'termina';

export type GuestOsType = typeof CROSTINI_TYPE|typeof PLUGIN_VM_TYPE|
    typeof ARC_VM_TYPE|typeof BRUSCHETTA_TYPE;

export function getVMNameForGuestOsType(guestOs: GuestOsType): string {
  return {
    [CROSTINI_TYPE]: TERMINA_VM_TYPE,
    [PLUGIN_VM_TYPE]: 'PvmDefault',
    [ARC_VM_TYPE]: ARC_VM_TYPE,
    [BRUSCHETTA_TYPE]: 'bru',
  }[guestOs];
}

export const VM_DEVICE_MICROPHONE = 'microphone';

export interface ShareableDevices {
  [VM_DEVICE_MICROPHONE]: boolean;
}

/**
 * |ipv4| below is null if the guest is not currently running.
 */
export interface ContainerInfo {
  id: GuestId;
  ipv4: string|null;
}

export interface GuestOsBrowserProxy {
  /**
   * @param paths Paths to sanitze.
   * @return Text to display in UI.
   */
  getGuestOsSharedPathsDisplayText(paths: string[]): Promise<string[]>;

  /**
   * @param vmName VM to stop sharing path with.
   * @param path Path to stop sharing.
   * @return Result of unsharing.
   */
  removeGuestOsSharedPath(vmName: string, path: string): Promise<boolean>;

  /** Called when page is ready. */
  notifyGuestOsSharedUsbDevicesPageReady(): void;

  /**
   * @param vmName VM to share device with.
   * @param containerName container to share device with.
   * @param guid Unique device identifier.
   * @param shared Whether device is currently shared with Crostini.
   */
  setGuestOsUsbDeviceShared(
      vmName: string, containerName: string, guid: string,
      shared: boolean): void;
}

let instance: GuestOsBrowserProxy|null = null;

export class GuestOsBrowserProxyImpl implements GuestOsBrowserProxy {
  static getInstance(): GuestOsBrowserProxy {
    return instance || (instance = new GuestOsBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: GuestOsBrowserProxy): void {
    instance = obj;
  }

  getGuestOsSharedPathsDisplayText(paths: string[]): Promise<string[]> {
    return sendWithPromise('getGuestOsSharedPathsDisplayText', paths);
  }

  removeGuestOsSharedPath(vmName: string, path: string): Promise<boolean> {
    return sendWithPromise('removeGuestOsSharedPath', vmName, path);
  }

  notifyGuestOsSharedUsbDevicesPageReady(): void {
    chrome.send('notifyGuestOsSharedUsbDevicesPageReady');
  }

  setGuestOsUsbDeviceShared(
      vmName: string, containerName: string, guid: string,
      shared: boolean): void {
    chrome.send(
        'setGuestOsUsbDeviceShared', [vmName, containerName, guid, shared]);
  }
}
