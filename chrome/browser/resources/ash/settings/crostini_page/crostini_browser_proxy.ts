// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the "Linux Apps" (Crostini) section
 * to install and uninstall Crostini.
 */

import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {GuestId, TERMINA_VM_TYPE} from '../guest_os/guest_os_browser_proxy.js';

// Identifiers for the default Crostini VM and container.
export const DEFAULT_CROSTINI_VM = TERMINA_VM_TYPE;
export const DEFAULT_CROSTINI_CONTAINER = 'penguin';

export const DEFAULT_CROSTINI_GUEST_ID: GuestId = {
  vm_name: DEFAULT_CROSTINI_VM,
  container_name: DEFAULT_CROSTINI_CONTAINER,
};

/**
 * These values should remain consistent with their C++ counterpart
 * (chrome/browser/ash/crostini/crostini_port_forwarder.h).
 */
export enum CrostiniPortProtocol {
  TCP = 0,
  UDP = 1,
}

/**
 * Note: key names are kept to match c++ style keys in prefs, they must stay in
 * sync.
 */
export interface CrostiniPortSetting {
  container_id: GuestId;
  container_name: string;
  is_active: boolean;
  label: string;
  port_number: number;
  protocol_type: CrostiniPortProtocol;
  vm_name: string;
}

export interface SliderTick {
  label: string;
  value: number;
}

export interface CrostiniDiskInfo {
  succeeded: boolean;
  canResize: boolean;
  isUserChosenSize: boolean;
  isLowSpaceAvailable: boolean;
  defaultIndex: number;
  ticks: SliderTick[];
}

/**
 * Note: key names are kept to match c++ style keys in prefs, they must stay in
 * sync.
 */
export interface CrostiniPortActiveSetting {
  port_number: number;
  protocol_type: CrostiniPortProtocol;
  container_id: GuestId;
}

export const PortState = {
  VALID: '',
  INVALID: loadTimeData.getString('crostiniPortForwardingAddError') as string,
  DUPLICATE: loadTimeData.getString('crostiniPortForwardingAddExisting') as
      string,
};

export const MIN_VALID_PORT_NUMBER = 1024;   // Minimum 16-bit integer value.
export const MAX_VALID_PORT_NUMBER = 65535;  // Maximum 16-bit integer value.

export interface CrostiniBrowserProxy {
  /* Show crostini installer. */
  requestCrostiniInstallerView(): void;

  /* Show remove crostini dialog. */
  requestRemoveCrostini(): void;

  /**
   * Request chrome send a crostini-installer-status-changed event with the
   * current installer status
   */
  requestCrostiniInstallerStatus(): void;

  /**
   * Request chrome send a crostini-export-import-operation-status-changed
   * event with the current operation status
   */
  requestCrostiniExportImportOperationStatus(): void;

  /**
   * Export crostini container.
   * @param containerId container id of container to export.
   */
  exportCrostiniContainer(containerId: GuestId): void;

  /**
   * Import crostini container.
   * @param containerId container id of container to import.
   */
  importCrostiniContainer(containerId: GuestId): void;

  /** Queries the current status of ARC ADB Sideloading. */
  requestArcAdbSideloadStatus(): void;

  /** Queries whether the user is allowed to enable ARC ADB Sideloading. */
  getCanChangeArcAdbSideloading(): void;

  /** Initiates the flow to enable ARC ADB Sideloading. */
  enableArcAdbSideload(): void;

  /** Initiates the flow to disable ARC ADB Sideloading. */
  disableArcAdbSideload(): void;

  /** Show the container upgrade UI. */
  requestCrostiniContainerUpgradeView(): void;

  /**
   * Request chrome send a crostini-upgrader-status-changed event with the
   * current upgrader dialog status
   */
  requestCrostiniUpgraderDialogStatus(): void;

  /**
   * Request chrome send a crostini-container-upgrade-available-changed event
   * with the availability of an upgrade for the container.
   */
  requestCrostiniContainerUpgradeAvailable(): void;

  /**
   * @param vmName Name of the VM to get disk info for.
   * @param requestFullInfo Whether to request full disk info, which
   *     can take several seconds because it requires starting the VM. Set to
   *     false for the main Crostini pages and true for the resize dialog.
   * @return The requested information.
   */
  getCrostiniDiskInfo(vmName: string, requestFullInfo: boolean):
      Promise<CrostiniDiskInfo>;

  /**
   * Resizes a preallocated user-chosen-size Crostini VM disk to the requested
   * size.
   * @param vmName Name of the VM to resize.
   * @param newSizeBytes Size in bytes to resize to.
   * @return Whether resizing succeeded(true) or failed.
   */
  resizeCrostiniDisk(vmName: string, newSizeBytes: number): Promise<boolean>;

  /**
   * Checks if a proposed change to mic sharing requires Crostini to be
   * restarted for it to take effect.
   * @param proposedValue Reflects what mic sharing is being set to.
   * @return Whether Crostini requires a restart or not.
   */
  checkCrostiniMicSharingStatus(proposedValue: boolean): Promise<boolean>;

  /**
   * @param containerId id of container to add port forwarding.
   * @param portNumber Port number to start forwarding.
   * @param protocol Networking protocol to use.
   * @param label Label for this port.
   * @return Whether the requested port was added and forwarded successfully.
   */
  addCrostiniPortForward(
      containerId: GuestId, portNumber: number, protocol: CrostiniPortProtocol,
      label: string): Promise<boolean>;

  /**
   * @param containerId id from which to remove port forwarding.
   * @param portNumber Port number to stop forwarding and remove.
   * @param protocol Networking protocol to use.
   * @return Whether requested port was deallocated and removed successfully.
   */
  removeCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocol: CrostiniPortProtocol): Promise<boolean>;

  /**
   * @param containerId id from which to remove all port forwarding.
   */
  removeAllCrostiniPortForwards(containerId: GuestId): void;

  /**
   * @param containerId id for which to activate port forward.
   * @param portNumber Existing port number to activate.
   * @param protocol Networking protocol for existing port rule to activate.
   * @return Whether the requested port was forwarded successfully
   */
  activateCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocol: CrostiniPortProtocol): Promise<boolean>;

  /**
   * @param containerId id for which to deactivate port forward.
   * @param portNumber Existing port number to activate.
   * @param protocol Networking protocol for existing port rule to deactivate.
   * @return Whether the requested port was deallocated successfully.
   */
  deactivateCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocol: CrostiniPortProtocol): Promise<boolean>;

  getCrostiniActivePorts(): Promise<CrostiniPortActiveSetting[]>;

  getCrostiniActiveNetworkInfo(): Promise<string[]>;

  checkCrostiniIsRunning(): Promise<boolean>;

  checkBruschettaIsRunning(): Promise<boolean>;

  /**
   * Shuts Crostini (Termina VM) down.
   */
  shutdownCrostini(): void;

  /**
   * Shuts Bruschetta (gLinux for ChromeOS) down.
   */
  shutdownBruschetta(): void;

  /**
   * @param enabled Set Crostini's access to the mic.
   */
  setCrostiniMicSharingEnabled(enabled: boolean): void;

  /**
   * @return Crostini's access to the mic.
   */
  getCrostiniMicSharingEnabled(): Promise<boolean>;

  /**
   * @param containerId id of container to create.
   * @param imageServer url of lxd container server from which to fetch
   * @param imageAlias name of image to fetch e.g. 'debian/bullseye'
   * @param containerFile file location of an Ansible playbook (.yaml)
   *     or a Crostini backup file (.tini, .tar.gz, .tar) to create the
   *     container from
   */
  createContainer(
      containerId: GuestId, imageServer: string|null, imageAlias: string|null,
      containerFile: string|null): void;

  /**
   * @param containerId id of container to delete.
   */
  deleteContainer(containerId: GuestId): void;

  /**
   * Fetches container info for all known containers and invokes listener
   * callback.
   */
  requestContainerInfo(): void;

  /**
   * @param containerId container id to update.
   * @param badgeColor new badge color for the container.
   */
  setContainerBadgeColor(containerId: GuestId, badgeColor: SkColor): void;

  /**
   * @param containerId id of container to stop, recovering CPU and
   * other resources.
   */
  stopContainer(containerId: GuestId): void;

  /**
   * Opens file selector dialog to allow user to select an Ansible playbook
   * to preconfigure their container.
   *
   * @return Returns a filepath to the selected file.
   */
  openContainerFileSelector(): Promise<string>;

  /**
   * Fetches vmdevice sharing info for all known containers and invokes listener
   * callback.
   */
  requestSharedVmDevices(): void;

  /**
   * @param id GuestId in question.
   * @param device VmDevice which might be shared.
   * @return Whether the device is shared.
   */
  isVmDeviceShared(id: GuestId, device: string): Promise<boolean>;

  /**
   * @param id GuestId in question.
   * @param device VmDevice which might be shared.
   * @param shared Whether to share the device with the guest.
   * @return Whether the sharing could be applied.
   */
  setVmDeviceShared(id: GuestId, device: string, shared: boolean):
      Promise<boolean>;

  /** Show Bruschetta installer. */
  requestBruschettaInstallerView(): void;

  /** Show Bruschetta uninstaller. */
  requestBruschettaUninstallerView(): void;
}

let instance: CrostiniBrowserProxy|null = null;

export class CrostiniBrowserProxyImpl implements CrostiniBrowserProxy {
  static getInstance(): CrostiniBrowserProxy {
    return instance || (instance = new CrostiniBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: CrostiniBrowserProxy): void {
    instance = obj;
  }

  requestCrostiniInstallerView(): void {
    chrome.send('requestCrostiniInstallerView');
  }

  requestRemoveCrostini(): void {
    chrome.send('requestRemoveCrostini');
  }

  requestCrostiniInstallerStatus(): void {
    chrome.send('requestCrostiniInstallerStatus');
  }

  requestCrostiniExportImportOperationStatus(): void {
    chrome.send('requestCrostiniExportImportOperationStatus');
  }

  exportCrostiniContainer(containerId: GuestId): void {
    chrome.send('exportCrostiniContainer', [containerId]);
  }

  importCrostiniContainer(containerId: GuestId): void {
    chrome.send('importCrostiniContainer', [containerId]);
  }

  requestArcAdbSideloadStatus(): void {
    chrome.send('requestArcAdbSideloadStatus');
  }

  getCanChangeArcAdbSideloading(): void {
    chrome.send('getCanChangeArcAdbSideloading');
  }

  enableArcAdbSideload(): void {
    chrome.send('enableArcAdbSideload');
  }

  disableArcAdbSideload(): void {
    chrome.send('disableArcAdbSideload');
  }

  requestCrostiniContainerUpgradeView(): void {
    chrome.send('requestCrostiniContainerUpgradeView');
  }

  requestCrostiniUpgraderDialogStatus(): void {
    chrome.send('requestCrostiniUpgraderDialogStatus');
  }

  requestCrostiniContainerUpgradeAvailable(): void {
    chrome.send('requestCrostiniContainerUpgradeAvailable');
  }

  getCrostiniDiskInfo(vmName: string, fullInfo: boolean):
      Promise<CrostiniDiskInfo> {
    return sendWithPromise('getCrostiniDiskInfo', vmName, fullInfo);
  }

  resizeCrostiniDisk(vmName: string, newSizeBytes: number): Promise<boolean> {
    return sendWithPromise('resizeCrostiniDisk', vmName, newSizeBytes);
  }

  checkCrostiniMicSharingStatus(proposedValue: boolean): Promise<boolean> {
    return sendWithPromise('checkCrostiniMicSharingStatus', proposedValue);
  }

  addCrostiniPortForward(
      containerId: GuestId, portNumber: number, protocol: CrostiniPortProtocol,
      label: string): Promise<boolean> {
    return sendWithPromise(
        'addCrostiniPortForward', containerId, portNumber, protocol, label);
  }

  removeCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocol: CrostiniPortProtocol): Promise<boolean> {
    return sendWithPromise(
        'removeCrostiniPortForward', containerId, portNumber, protocol);
  }

  removeAllCrostiniPortForwards(containerId: GuestId): void {
    chrome.send('removeAllCrostiniPortForwards', [containerId]);
  }

  activateCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocol: CrostiniPortProtocol): Promise<boolean> {
    return sendWithPromise(
        'activateCrostiniPortForward', containerId, portNumber, protocol);
  }

  deactivateCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocol: CrostiniPortProtocol): Promise<boolean> {
    return sendWithPromise(
        'deactivateCrostiniPortForward', containerId, portNumber, protocol);
  }

  getCrostiniActivePorts(): Promise<CrostiniPortActiveSetting[]> {
    return sendWithPromise('getCrostiniActivePorts');
  }

  getCrostiniActiveNetworkInfo(): Promise<string[]> {
    return sendWithPromise('getCrostiniActiveNetworkInfo');
  }

  checkCrostiniIsRunning(): Promise<boolean> {
    return sendWithPromise('checkCrostiniIsRunning');
  }

  checkBruschettaIsRunning(): Promise<boolean> {
    return sendWithPromise('checkBruschettaIsRunning');
  }

  shutdownCrostini(): void {
    chrome.send('shutdownCrostini');
  }

  shutdownBruschetta(): void {
    chrome.send('shutdownBruschetta');
  }

  setCrostiniMicSharingEnabled(enabled: boolean): void {
    chrome.send('setCrostiniMicSharingEnabled', [enabled]);
  }

  getCrostiniMicSharingEnabled(): Promise<boolean> {
    return sendWithPromise('getCrostiniMicSharingEnabled');
  }

  createContainer(
      containerId: GuestId, imageServer: string|null, imageAlias: string|null,
      containerFile: string|null): void {
    chrome.send(
        'createContainer',
        [containerId, imageServer, imageAlias, containerFile]);
  }

  deleteContainer(containerId: GuestId): void {
    chrome.send('deleteContainer', [containerId]);
  }

  requestContainerInfo(): void {
    chrome.send('requestContainerInfo');
  }

  setContainerBadgeColor(containerId: GuestId, badgeColor: SkColor): void {
    chrome.send('setContainerBadgeColor', [containerId, badgeColor]);
  }

  stopContainer(containerId: GuestId): void {
    chrome.send('stopContainer', [containerId]);
  }

  openContainerFileSelector(): Promise<string> {
    return sendWithPromise('openContainerFileSelector');
  }

  requestSharedVmDevices(): void {
    chrome.send('requestSharedVmDevices');
  }

  isVmDeviceShared(id: GuestId, device: string): Promise<boolean> {
    return sendWithPromise('isVmDeviceShared', id, device);
  }

  setVmDeviceShared(id: GuestId, device: string, shared: boolean):
      Promise<boolean> {
    return sendWithPromise('setVmDeviceShared', id, device, shared);
  }

  requestBruschettaInstallerView(): void {
    chrome.send('requestBruschettaInstallerView');
  }

  requestBruschettaUninstallerView(): void {
    chrome.send('requestBruschettaUninstallerView');
  }
}
