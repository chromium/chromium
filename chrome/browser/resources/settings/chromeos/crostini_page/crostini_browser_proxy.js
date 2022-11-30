// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

import {loadTimeData} from '../../i18n_setup.js';
import {GuestId} from '../guest_os/guest_os_browser_proxy.js';

// Identifiers for the default Crostini VM and container.
/** @type {string} */ export const DEFAULT_CROSTINI_VM = 'termina';
/** @type {string} */ export const DEFAULT_CROSTINI_CONTAINER = 'penguin';

/** @type {!GuestId} */ export const DEFAULT_CROSTINI_GUEST_ID = {
  vm_name: DEFAULT_CROSTINI_VM,
  container_name: DEFAULT_CROSTINI_CONTAINER,
};

/**
 * These values should remain consistent with their C++ counterpart
 * (chrome/browser/ash/crostini/crostini_port_forwarder.h).
 * @enum {number}
 */
export const CrostiniPortProtocol = {
  TCP: 0,
  UDP: 1,
};

/**
 * Note: key names are kept to match c++ style keys in prefs, they must stay in
 * sync.
 * @typedef {{label: string,
 *            port_number: number,
 *            protocol_type: !CrostiniPortProtocol,
 *            container_id: !GuestId}}
 */
export let CrostiniPortSetting;

/**
 * @typedef {{succeeded: boolean,
 *            canResize: boolean,
 *            isUserChosenSize: boolean,
 *            isLowSpaceAvailable: boolean,
 *            defaultIndex: number,
 *            ticks: !Array}}
 */
export let CrostiniDiskInfo;

/**
 * Note: key names are kept to match c++ style keys in prefs, they must stay in
 * sync.
 * @typedef {{port_number: number,
 *            protocol_type: !CrostiniPortProtocol,
 *            container_id: !GuestId}}
 */
export let CrostiniPortActiveSetting;

/**
 * @enum {string}
 */
export const PortState = {
  VALID: '',
  INVALID: loadTimeData.getString('crostiniPortForwardingAddError'),
  DUPLICATE: loadTimeData.getString('crostiniPortForwardingAddExisting'),
};

export const MIN_VALID_PORT_NUMBER = 1024;   // Minimum 16-bit integer value.
export const MAX_VALID_PORT_NUMBER = 65535;  // Maximum 16-bit integer value.

/**
 * @fileoverview A helper object used by the "Linux Apps" (Crostini) section
 * to install and uninstall Crostini.
 */

/** @interface */
export class CrostiniBrowserProxy {
  /* Show crostini installer. */
  requestCrostiniInstallerView() {}

  /* Show remove crostini dialog. */
  requestRemoveCrostini() {}

  /**
   * Request chrome send a crostini-installer-status-changed event with the
   * current installer status
   */
  requestCrostiniInstallerStatus() {}

  /**
   * Request chrome send a crostini-export-import-operation-status-changed
   * event with the current operation status
   */
  requestCrostiniExportImportOperationStatus() {}

  /**
   * Export crostini container.
   * @param {!GuestId} containerId container id of container to export.
   */
  exportCrostiniContainer(containerId) {}

  /**
   * Import crostini container.
   * @param {!GuestId} containerId container id of container to import.
   */
  importCrostiniContainer(containerId) {}

  /** Queries the current status of ARC ADB Sideloading. */
  requestArcAdbSideloadStatus() {}

  /** Queries whether the user is allowed to enable ARC ADB Sideloading. */
  getCanChangeArcAdbSideloading() {}

  /** Initiates the flow to enable ARC ADB Sideloading. */
  enableArcAdbSideload() {}

  /** Initiates the flow to disable ARC ADB Sideloading. */
  disableArcAdbSideload() {}

  /** Show the container upgrade UI. */
  requestCrostiniContainerUpgradeView() {}

  /**
   * Request chrome send a crostini-upgrader-status-changed event with the
   * current upgrader dialog status
   */
  requestCrostiniUpgraderDialogStatus() {}

  /**
   * Request chrome send a crostini-container-upgrade-available-changed event
   * with the availability of an upgrade for the container.
   */
  requestCrostiniContainerUpgradeAvailable() {}

  /**
   * @param {string} vmName Name of the VM to get disk info for.
   * @param {boolean} requestFullInfo Whether to request full disk info, which
   *     can take several seconds because it requires starting the VM. Set to
   *     false for the main Crostini pages and true for the resize dialog.
   * @return {!Promise<CrostiniDiskInfo>} The requested information.
   */
  getCrostiniDiskInfo(vmName, requestFullInfo) {}

  /**
   * Resizes a preallocated user-chosen-size Crostini VM disk to the requested
   * size.
   * @param {string} vmName Name of the VM to resize.
   * @param {number} newSizeBytes Size in bytes to resize to.
   * @return {!Promise<boolean>} Whether resizing succeeded(true) or failed.
   */
  resizeCrostiniDisk(vmName, newSizeBytes) {}

  /**
   * Checks if a proposed change to mic sharing requires Crostini to be
   * restarted for it to take effect.
   * @param {boolean} proposedValue Reflects what mic sharing is being set
   *     to.
   * @return {!Promise<boolean>} Whether Crostini requires a restart or not.
   */
  checkCrostiniMicSharingStatus(proposedValue) {}

  /**
   * @param {!GuestId} containerId id of container to add port forwarding.
   * @param {number} portNumber Port number to start forwarding.
   * @param {!CrostiniPortProtocol} protocol Networking protocol to use.
   * @param {string} label Label for this port.
   * @return {!Promise<boolean>} Whether the requested port was added and
   * forwarded successfully.
   */
  addCrostiniPortForward(containerId, portNumber, protocol, label) {}

  /**
   * @param {!GuestId} containerId id from which to remove port forwarding.
   * @param {number} portNumber Port number to stop forwarding and remove.
   * @param {!CrostiniPortProtocol} protocol Networking protocol to use.
   * @return {!Promise<boolean>} Whether requested port was deallocated and
   * removed successfully.
   */
  removeCrostiniPortForward(containerId, portNumber, protocol) {}

  /**
   * @param {!GuestId} containerId id from which to remove all port
   *     forwarding.
   */
  removeAllCrostiniPortForwards(containerId) {}

  /**
   * @param {!GuestId} containerId id for which to activate port forward.
   * @param {number} portNumber Existing port number to activate.
   * @param {!CrostiniPortProtocol} protocol Networking protocol for existing
   * port rule to activate.
   * @return {!Promise<boolean>} Whether the requested port was forwarded
   * successfully
   */
  activateCrostiniPortForward(containerId, portNumber, protocol) {}

  /**
   * @param {!GuestId} containerId id for which to deactivate port forward.
   * @param {number} portNumber Existing port number to activate.
   * @param {!CrostiniPortProtocol} protocol Networking protocol for existing
   * port rule to deactivate.
   * @return {!Promise<boolean>} Whether the requested port was deallocated
   * successfully.
   */
  deactivateCrostiniPortForward(containerId, portNumber, protocol) {}

  /**
   * @return {!Promise<!Array<CrostiniPortActiveSetting>>}
   */
  getCrostiniActivePorts() {}

  /**
   * @return {!Promise<boolean>}
   */
  checkCrostiniIsRunning() {}

  /**
   * Shuts Crostini (Termina VM) down.
   */
  shutdownCrostini() {}

  /**
   * @param {boolean} enabled Set Crostini's access to the mic.
   */
  setCrostiniMicSharingEnabled(enabled) {}

  /**
   * @return {!Promise<boolean>} Return Crostini's access to the mic.
   */
  getCrostiniMicSharingEnabled() {}

  /**
   * @param {!GuestId} containerId id of container to create.
   * @param {?string} imageServer url of lxd container server from which to
   *     fetch
   * @param {?string} imageAlias name of image to fetch e.g. 'debian/bullseye'
   * @param {?string} containerFile file location of an Ansible playbook (.yaml)
   *     or a Crostini backup file (.tini, .tar.gz, .tar) to create the
   *     container from
   */
  createContainer(containerId, imageServer, imageAlias, containerFile) {}

  /**
   * @param {!GuestId} containerId id of container to delete.
   */
  deleteContainer(containerId) {}

  /**
   * Fetches container info for all known containers and invokes listener
   * callback.
   */
  requestContainerInfo() {}

  /**
   * @param {!GuestId} containerId container id to update.
   * @param {!skia.mojom.SkColor} badge_color new badge color for the container.
   */
  setContainerBadgeColor(containerId, badge_color) {}

  /**
   * @param {!GuestId} containerId id of container to stop, recovering
   * CPU and other resources.
   */
  stopContainer(containerId) {}

  /**
   * Opens file selector dialog to allow user to select an Ansible playbook
   * to preconfigure their container.
   *
   * @return {!Promise<string>} Returns a filepath to the selected file.
   */
  openContainerFileSelector() {}
}

/** @type {?CrostiniBrowserProxy} */
let instance = null;

/**
 * @implements {CrostiniBrowserProxy}
 */
export class CrostiniBrowserProxyImpl {
  /** @return {!CrostiniBrowserProxy} */
  static getInstance() {
    return instance || (instance = new CrostiniBrowserProxyImpl());
  }

  /** @param {!CrostiniBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  requestCrostiniInstallerView() {
    chrome.send('requestCrostiniInstallerView');
  }

  /** @override */
  requestRemoveCrostini() {
    chrome.send('requestRemoveCrostini');
  }

  /** @override */
  requestCrostiniInstallerStatus() {
    chrome.send('requestCrostiniInstallerStatus');
  }

  /** @override */
  requestCrostiniExportImportOperationStatus() {
    chrome.send('requestCrostiniExportImportOperationStatus');
  }

  /** @override */
  exportCrostiniContainer(containerId) {
    chrome.send('exportCrostiniContainer', [containerId]);
  }

  /** @override */
  importCrostiniContainer(containerId) {
    chrome.send('importCrostiniContainer', [containerId]);
  }

  /** @override */
  requestArcAdbSideloadStatus() {
    chrome.send('requestArcAdbSideloadStatus');
  }

  /** @override */
  getCanChangeArcAdbSideloading() {
    chrome.send('getCanChangeArcAdbSideloading');
  }

  /** @override */
  enableArcAdbSideload() {
    chrome.send('enableArcAdbSideload');
  }

  /** @override */
  disableArcAdbSideload() {
    chrome.send('disableArcAdbSideload');
  }

  /** @override */
  requestCrostiniContainerUpgradeView() {
    chrome.send('requestCrostiniContainerUpgradeView');
  }

  /** @override */
  requestCrostiniUpgraderDialogStatus() {
    chrome.send('requestCrostiniUpgraderDialogStatus');
  }

  /** @override */
  requestCrostiniContainerUpgradeAvailable() {
    chrome.send('requestCrostiniContainerUpgradeAvailable');
  }

  /** @override */
  getCrostiniDiskInfo(vmName, fullInfo) {
    return sendWithPromise('getCrostiniDiskInfo', vmName, fullInfo);
  }

  /** @override */
  resizeCrostiniDisk(vmName, newSizeBytes) {
    return sendWithPromise('resizeCrostiniDisk', vmName, newSizeBytes);
  }

  /** @override */
  checkCrostiniMicSharingStatus(proposedValue) {
    return sendWithPromise('checkCrostiniMicSharingStatus', proposedValue);
  }

  /** @override */
  addCrostiniPortForward(containerId, portNumber, protocol, label) {
    return sendWithPromise(
        'addCrostiniPortForward', containerId, portNumber, protocol, label);
  }

  /** override */
  removeCrostiniPortForward(containerId, portNumber, protocol) {
    return sendWithPromise(
        'removeCrostiniPortForward', containerId, portNumber, protocol);
  }

  /** override */
  removeAllCrostiniPortForwards(containerId) {
    chrome.send('removeAllCrostiniPortForwards', [containerId]);
  }

  /** override */
  activateCrostiniPortForward(containerId, portNumber, protocol) {
    return sendWithPromise(
        'activateCrostiniPortForward', containerId, portNumber, protocol);
  }

  /** @override */
  deactivateCrostiniPortForward(containerId, portNumber, protocol) {
    return sendWithPromise(
        'deactivateCrostiniPortForward', containerId, portNumber, protocol);
  }

  /** @override */
  getCrostiniActivePorts() {
    return sendWithPromise('getCrostiniActivePorts');
  }

  /** @override */
  checkCrostiniIsRunning() {
    return sendWithPromise('checkCrostiniIsRunning');
  }

  /** @override */
  shutdownCrostini() {
    chrome.send('shutdownCrostini');
  }

  /** @override */
  setCrostiniMicSharingEnabled(enabled) {
    chrome.send('setCrostiniMicSharingEnabled', [enabled]);
  }

  /** @override */
  getCrostiniMicSharingEnabled() {
    return sendWithPromise('getCrostiniMicSharingEnabled');
  }

  /** @override */
  createContainer(containerId, imageServer, imageAlias, containerFile) {
    chrome.send(
        'createContainer',
        [containerId, imageServer, imageAlias, containerFile]);
  }

  /** @override */
  deleteContainer(containerId) {
    chrome.send('deleteContainer', [containerId]);
  }

  /** @override */
  requestContainerInfo() {
    return chrome.send('requestContainerInfo');
  }

  /** @override */
  setContainerBadgeColor(containerId, badge_color) {
    chrome.send('setContainerBadgeColor', [containerId, badge_color]);
  }

  /** @override */
  stopContainer(containerId) {
    chrome.send('stopContainer', [containerId]);
  }

  /** @override */
  openContainerFileSelector() {
    return sendWithPromise('openContainerFileSelector');
  }
}
