// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



// Identifiers for the default Crostini VM and container.
/** @type {string} */ const DEFAULT_CROSTINI_VM = 'termina';
/** @type {string} */ const DEFAULT_CROSTINI_CONTAINER = 'penguin';


/**
 * These values should remain consistent with their C++ counterpart
 * (chrome/browser/chromeos/crostini/crostini_port_forwarder.h).
 * @enum {number}
 */
const CrostiniPortProtocol = {
  TCP: 0,
  UDP: 1,
};

/**
 * @typedef {{path: string,
 *            pathDisplayText: string}}
 */
let CrostiniSharedPath;

/**
 * @typedef {{label: string,
 *            guid: string,
 *            shared: boolean,
 *            shareWillReassign: boolean}}
 */
let CrostiniSharedUsbDevice;

/**
 * @typedef {{label: string,
 *            port_number: number,
 *            protocol_type: !CrostiniPortProtocol}}
 */
let CrostiniPortSetting;

/**
 * @typedef {{succeeded: boolean,
 *            canResize: boolean,
 *            isUserChosenSize: boolean,
 *            isLowSpaceAvailable: boolean,
 *            defaultIndex: number,
 *            ticks: !Array}}
 */
let CrostiniDiskInfo;

/**
 * @typedef {{port_number: number,
 *            protocol_type: !CrostiniPortProtocol}}
 */
let CrostiniPortActiveSetting;

/**
 * @fileoverview A helper object used by the "Linux Apps" (Crostini) section
 * to install and uninstall Crostini.
 */
cr.define('settings', function() {
  /** @interface */
  class CrostiniBrowserProxy {
    /* Show crostini installer. */
    requestCrostiniInstallerView() {}

    /* Show remove crostini dialog. */
    requestRemoveCrostini() {}

    /**
     * @param {!Array<string>} paths Paths to sanitze.
     * @return {!Promise<!Array<string>>} Text to display in UI.
     */
    getCrostiniSharedPathsDisplayText(paths) {}

    /** Called when page is ready. */
    notifyCrostiniSharedUsbDevicesPageReady() {}

    /**
     * @param {string} guid Unique device identifier.
     * @param {boolean} shared Whether device is currently shared with Crostini.
     */
    setCrostiniUsbDeviceShared(guid, shared) {}

    /**
     * @param {string} vmName VM to stop sharing path with.
     * @param {string} path Path to stop sharing.
     * @return {!Promise<boolean>} Result of unsharing.
     */
    removeCrostiniSharedPath(vmName, path) {}

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
     */
    exportCrostiniContainer() {}

    /**
     * Import crostini container.
     */
    importCrostiniContainer() {}

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
     * @param {string} vmName Name of vm to add port forwarding for.
     * @param {string} containerName Name of container to add port forwarding
     *     for.
     * @param {number} portNumber Port number to start forwarding.
     * @param {!CrostiniPortProtocol} protocol Networking protocol to use.
     * @param {string} label Label for this port.
     * @return {!Promise<boolean>} Whether the requested port was added and
     * forwarded successfully.
     */
    addCrostiniPortForward(vmName, containerName, portNumber, protocol, label) {
    }

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
     * @param {string} vmName Name of vm to remove port forwarding for.
     * @param {string} containerName Name of container to remove port forwarding
     *     for.
     * @param {number} portNumber Port number to stop forwarding and remove.
     * @param {!CrostiniPortProtocol} protocol Networking protocol to use.
     * @return {!Promise<boolean>} Whether requested port was deallocated and
     * removed successfully.
     */
    removeCrostiniPortForward(vmName, containerName, portNumber, protocol) {}

    /**
     * @param {string} vmName Name of vm to remove all port forwarding for.
     * @param {string} containerName Name of container to remove all port
     *     forwarding for.
     */
    removeAllCrostiniPortForwards(vmName, containerName) {}

    /**
     * @param {string} vmName Name of vm to activate port forwarding for.
     * @param {string} containerName Name of container to activate port
     *     forwarding for.
     * @param {number} portNumber Existing port number to activate.
     * @param {!CrostiniPortProtocol} protocol Networking protocol for existing
     * port rule to activate.
     * @return {!Promise<boolean>} Whether the requested port was forwarded
     * successfully
     */
    activateCrostiniPortForward(vmName, containerName, portNumber, protocol) {}

    /**
     * @param {string} vmName Name of vm to activate port forwarding for.
     * @param {string} containerName Name of container to activate port
     *     forwarding for.
     * @param {number} portNumber Existing port number to activate.
     * @param {!CrostiniPortProtocol} protocol Networking protocol for existing
     * port rule to deactivate.
     * @return {!Promise<boolean>} Whether the requested port was deallocated
     * successfully.
     */
    deactivateCrostiniPortForward(vmName, containerName, portNumber, protocol) {
    }

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
  }

  /** @implements {settings.CrostiniBrowserProxy} */
  class CrostiniBrowserProxyImpl {
    /** @override */
    requestCrostiniInstallerView() {
      chrome.send('requestCrostiniInstallerView');
    }

    /** @override */
    requestRemoveCrostini() {
      chrome.send('requestRemoveCrostini');
    }

    /** @override */
    getCrostiniSharedPathsDisplayText(paths) {
      return cr.sendWithPromise('getCrostiniSharedPathsDisplayText', paths);
    }

    /** @override */
    notifyCrostiniSharedUsbDevicesPageReady() {
      return cr.sendWithPromise('notifyCrostiniSharedUsbDevicesPageReady');
    }

    /** @override */
    setCrostiniUsbDeviceShared(guid, shared) {
      return chrome.send('setCrostiniUsbDeviceShared', [guid, shared]);
    }

    /** @override */
    removeCrostiniSharedPath(vmName, path) {
      return cr.sendWithPromise('removeCrostiniSharedPath', vmName, path);
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
    exportCrostiniContainer() {
      chrome.send('exportCrostiniContainer');
    }

    /** @override */
    importCrostiniContainer() {
      chrome.send('importCrostiniContainer');
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
    addCrostiniPortForward(vmName, containerName, portNumber, protocol, label) {
      return cr.sendWithPromise(
          'addCrostiniPortForward', vmName, containerName, portNumber, protocol,
          label);
    }

    /** @override */
    getCrostiniDiskInfo(vmName, fullInfo) {
      return cr.sendWithPromise('getCrostiniDiskInfo', vmName, fullInfo);
    }

    /** @override */
    resizeCrostiniDisk(vmName, newSizeBytes) {
      return cr.sendWithPromise('resizeCrostiniDisk', vmName, newSizeBytes);
    }

    /** @override */
    checkCrostiniMicSharingStatus(proposedValue) {
      return cr.sendWithPromise('checkCrostiniMicSharingStatus', proposedValue);
    }

    /** override */
    removeCrostiniPortForward(vmName, containerName, portNumber, protocol) {
      return cr.sendWithPromise(
          'removeCrostiniPortForward', vmName, containerName, portNumber,
          protocol);
    }

    /** override */
    removeAllCrostiniPortForwards(vmName, containerName) {
      chrome.send('removeAllCrostiniPortForwards', [vmName, containerName]);
    }

    /** override */
    activateCrostiniPortForward(vmName, containerName, portNumber, protocol) {
      return cr.sendWithPromise(
          'activateCrostiniPortForward', vmName, containerName, portNumber,
          protocol);
    }

    /** @override */
    deactivateCrostiniPortForward(vmName, containerName, portNumber, protocol) {
      return cr.sendWithPromise(
          'deactivateCrostiniPortForward', vmName, containerName, portNumber,
          protocol);
    }

    /** @override */
    getCrostiniActivePorts() {
      return cr.sendWithPromise('getCrostiniActivePorts');
    }

    /** @override */
    checkCrostiniIsRunning() {
      return cr.sendWithPromise('checkCrostiniIsRunning');
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
      return cr.sendWithPromise('getCrostiniMicSharingEnabled');
    }
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(CrostiniBrowserProxyImpl);

  // #cr_define_end
  return {
    CrostiniBrowserProxy: CrostiniBrowserProxy,
    CrostiniBrowserProxyImpl: CrostiniBrowserProxyImpl,
  };
});
