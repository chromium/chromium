// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{path: string,
 *            pathDisplayText: string}}
 */
let CrostiniSharedPath;

/**
 * @typedef {{label: string,
 *            guid: string,
 *            shared: boolean}}
 */
let CrostiniSharedUsbDevice;

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

    /**
     * @return {!Promise<!Array<CrostiniSharedUsbDevice>>}
     */
    getCrostiniSharedUsbDevices() {}

    /**
     * @param {string} guid Unique device identifier.
     * @param {boolean} shared Whether device is currently shared with Crostini.
     */
    setCrostiniUsbDeviceShared(guid, shared) {}

    /**
     * @param {string} vmName VM to stop sharing path with.
     * @param {string} path Path to stop sharing.
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

    /** Initiates the flow to enable ARC ADB Sideloading. */
    enableArcAdbSideload() {}

    /** Initiates the flow to disable ARC ADB Sideloading. */
    disableArcAdbSideload() {}
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
    getCrostiniSharedUsbDevices() {
      return cr.sendWithPromise('getCrostiniSharedUsbDevices');
    }

    /** @override */
    setCrostiniUsbDeviceShared(guid, shared) {
      return chrome.send('setCrostiniUsbDeviceShared', [guid, shared]);
    }

    /** @override */
    removeCrostiniSharedPath(vmName, path) {
      chrome.send('removeCrostiniSharedPath', [vmName, path]);
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
    enableArcAdbSideload() {
      chrome.send('enableArcAdbSideload');
    }

    /** @override */
    disableArcAdbSideload() {
      chrome.send('disableArcAdbSideload');
    }
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(CrostiniBrowserProxyImpl);

  return {
    CrostiniBrowserProxy: CrostiniBrowserProxy,
    CrostiniBrowserProxyImpl: CrostiniBrowserProxyImpl,
  };
});
