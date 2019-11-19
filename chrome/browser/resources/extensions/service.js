// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {ActivityLogDelegate} from './activity_log/activity_log_history.js';
import {ActivityLogEventDelegate} from './activity_log/activity_log_stream.js';
import {ErrorPageDelegate} from './error_page.js';
import {ItemDelegate} from './item.js';
import {KeyboardShortcutDelegate} from './keyboard_shortcut_delegate.js';
import {LoadErrorDelegate} from './load_error.js';
import {Dialog, navigation, Page} from './navigation_helper.js';
import {PackDialogDelegate} from './pack_dialog.js';
import {ToolbarDelegate} from './toolbar.js';


/**
 * @implements {ActivityLogDelegate}
 * @implements {ActivityLogEventDelegate}
 * @implements {ErrorPageDelegate}
 * @implements {ItemDelegate}
 * @implements {KeyboardShortcutDelegate}
 * @implements {LoadErrorDelegate}
 * @implements {PackDialogDelegate}
 * @implements {ToolbarDelegate}
 */
export class Service {
  constructor() {
    /** @private {boolean} */
    this.isDeleting_ = false;

    /** @private {!Set<string>} */
    this.eventsToIgnoreOnce_ = new Set();
  }

  getProfileConfiguration() {
    return new Promise(function(resolve, reject) {
      chrome.developerPrivate.getProfileConfiguration(resolve);
    });
  }

  getItemStateChangedTarget() {
    return chrome.developerPrivate.onItemStateChanged;
  }

  /**
   * @param {string} extensionId
   * @param {!chrome.developerPrivate.EventType} eventType
   * @return {boolean}
   */
  shouldIgnoreUpdate(extensionId, eventType) {
    return this.eventsToIgnoreOnce_.delete(`${extensionId}_${eventType}`);
  }

  /**
   * @param {string} extensionId
   * @param {!chrome.developerPrivate.EventType} eventType
   */
  ignoreNextEvent(extensionId, eventType) {
    this.eventsToIgnoreOnce_.add(`${extensionId}_${eventType}`);
  }

  getProfileStateChangedTarget() {
    return chrome.developerPrivate.onProfileStateChanged;
  }

  getExtensionsInfo() {
    return new Promise(function(resolve, reject) {
      chrome.developerPrivate.getExtensionsInfo(
          {includeDisabled: true, includeTerminated: true}, resolve);
    });
  }

  /** @override */
  getExtensionSize(id) {
    return new Promise(function(resolve, reject) {
      chrome.developerPrivate.getExtensionSize(id, resolve);
    });
  }

  /** @override */
  addRuntimeHostPermission(id, host) {
    return new Promise((resolve, reject) => {
      chrome.developerPrivate.addHostPermission(id, host, () => {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError.message);
          return;
        }
        resolve();
      });
    });
  }

  /** @override */
  removeRuntimeHostPermission(id, host) {
    return new Promise((resolve, reject) => {
      chrome.developerPrivate.removeHostPermission(id, host, () => {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError.message);
          return;
        }
        resolve();
      });
    });
  }

  /**
   * Opens a file browser dialog for the user to select a file (or directory).
   * @param {chrome.developerPrivate.SelectType} selectType
   * @param {chrome.developerPrivate.FileType} fileType
   * @return {Promise<string>} The promise to be resolved with the selected
   *     path.
   */
  chooseFilePath_(selectType, fileType) {
    return new Promise(function(resolve, reject) {
      chrome.developerPrivate.choosePath(selectType, fileType, function(path) {
        if (chrome.runtime.lastError &&
            chrome.runtime.lastError != 'File selection was canceled.') {
          reject(chrome.runtime.lastError);
        } else {
          resolve(path || '');
        }
      });
    });
  }

  /** @override */
  updateExtensionCommandKeybinding(extensionId, commandName, keybinding) {
    chrome.developerPrivate.updateExtensionCommand({
      extensionId: extensionId,
      commandName: commandName,
      keybinding: keybinding,
    });
  }

  /** @override */
  updateExtensionCommandScope(extensionId, commandName, scope) {
    // The COMMAND_REMOVED event needs to be ignored since it is sent before
    // the command is added back with the updated scope but can be handled
    // after the COMMAND_ADDED event.
    this.ignoreNextEvent(
        extensionId, chrome.developerPrivate.EventType.COMMAND_REMOVED);
    chrome.developerPrivate.updateExtensionCommand({
      extensionId: extensionId,
      commandName: commandName,
      scope: scope,
    });
  }


  /** @override */
  setShortcutHandlingSuspended(isCapturing) {
    chrome.developerPrivate.setShortcutHandlingSuspended(isCapturing);
  }

  /**
   * @param {chrome.developerPrivate.LoadUnpackedOptions=} opt_options
   * @return {!Promise} A signal that loading finished, rejected if any error
   *     occurred.
   * @private
   */
  loadUnpackedHelper_(opt_options) {
    return new Promise(function(resolve, reject) {
      const options = Object.assign(
          {
            failQuietly: true,
            populateError: true,
          },
          opt_options);

      chrome.developerPrivate.loadUnpacked(options, (loadError) => {
        if (chrome.runtime.lastError &&
            chrome.runtime.lastError.message !=
                'File selection was canceled.') {
          throw new Error(chrome.runtime.lastError.message);
        }
        if (loadError) {
          return reject(loadError);
        }

        resolve();
      });
    });
  }

  /** @override */
  deleteItem(id) {
    if (this.isDeleting_) {
      return;
    }
    this.isDeleting_ = true;
    chrome.management.uninstall(id, {showConfirmDialog: true}, () => {
      // The "last error" was almost certainly the user canceling the dialog.
      // Do nothing. We only check it so we don't get noisy logs.
      /** @suppress {suspiciousCode} */
      chrome.runtime.lastError;
      this.isDeleting_ = false;
    });
  }

  /** @override */
  setItemEnabled(id, isEnabled) {
    chrome.management.setEnabled(id, isEnabled);
  }

  /** @override */
  setItemAllowedIncognito(id, isAllowedIncognito) {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      incognitoAccess: isAllowedIncognito,
    });
  }

  /** @override */
  setItemAllowedOnFileUrls(id, isAllowedOnFileUrls) {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      fileAccess: isAllowedOnFileUrls,
    });
  }

  /** @override */
  setItemHostAccess(id, hostAccess) {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      hostAccess: hostAccess,
    });
  }

  /** @override */
  setItemCollectsErrors(id, collectsErrors) {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      errorCollection: collectsErrors,
    });
  }

  /** @override */
  inspectItemView(id, view) {
    chrome.developerPrivate.openDevTools({
      extensionId: id,
      renderProcessId: view.renderProcessId,
      renderViewId: view.renderViewId,
      incognito: view.incognito,
    });
  }

  /**
   * @param {string} url
   * @override
   */
  openUrl(url) {
    window.open(url);
  }

  /** @override */
  reloadItem(id) {
    return new Promise(function(resolve, reject) {
      chrome.developerPrivate.reload(
          id, {failQuietly: true, populateErrorForUnpacked: true},
          (loadError) => {
            if (loadError) {
              reject(loadError);
              return;
            }

            resolve();
          });
    });
  }

  /** @override */
  repairItem(id) {
    chrome.developerPrivate.repairExtension(id);
  }

  /** @override */
  showItemOptionsPage(extension) {
    assert(extension && extension.optionsPage);
    if (extension.optionsPage.openInTab) {
      chrome.developerPrivate.showOptions(extension.id);
    } else {
      navigation.navigateTo({
        page: Page.DETAILS,
        subpage: Dialog.OPTIONS,
        extensionId: extension.id,
      });
    }
  }

  /** @override */
  setProfileInDevMode(inDevMode) {
    chrome.developerPrivate.updateProfileConfiguration(
        {inDeveloperMode: inDevMode});
  }

  /** @override */
  loadUnpacked() {
    return this.loadUnpackedHelper_();
  }

  /** @override */
  retryLoadUnpacked(retryGuid) {
    // Attempt to load an unpacked extension, optionally as another attempt at
    // a previously-specified load.
    return this.loadUnpackedHelper_({retryGuid: retryGuid});
  }

  /** @override */
  choosePackRootDirectory() {
    return this.chooseFilePath_(
        chrome.developerPrivate.SelectType.FOLDER,
        chrome.developerPrivate.FileType.LOAD);
  }

  /** @override */
  choosePrivateKeyPath() {
    return this.chooseFilePath_(
        chrome.developerPrivate.SelectType.FILE,
        chrome.developerPrivate.FileType.PEM);
  }

  /** @override */
  packExtension(rootPath, keyPath, flag, callback) {
    chrome.developerPrivate.packDirectory(rootPath, keyPath, flag, callback);
  }

  /** @override */
  updateAllExtensions() {
    return new Promise((resolve) => {
      chrome.developerPrivate.autoUpdate(resolve);
      chrome.metricsPrivate.recordUserAction('Options_UpdateExtensions');
    });
  }

  /** @override */
  deleteErrors(extensionId, errorIds, type) {
    chrome.developerPrivate.deleteExtensionErrors({
      extensionId: extensionId,
      errorIds: errorIds,
      type: type,
    });
  }

  /** @override */
  requestFileSource(args) {
    return new Promise(function(resolve, reject) {
      chrome.developerPrivate.requestFileSource(args, resolve);
    });
  }

  /** @override */
  showInFolder(id) {
    chrome.developerPrivate.showPath(id);
  }

  /** @override */
  getExtensionActivityLog(extensionId) {
    return new Promise(function(resolve, reject) {
      chrome.activityLogPrivate.getExtensionActivities(
          {
            activityType: chrome.activityLogPrivate.ExtensionActivityFilter.ANY,
            extensionId: extensionId
          },
          resolve);
    });
  }

  /** @override */
  getFilteredExtensionActivityLog(extensionId, searchTerm) {
    const anyType = chrome.activityLogPrivate.ExtensionActivityFilter.ANY;

    // Construct one filter for each API call we will make: one for substring
    // search by api call, one for substring search by page URL, and one for
    // substring search by argument URL. % acts as a wildcard.
    const activityLogFilters = [
      {
        activityType: anyType,
        extensionId: extensionId,
        apiCall: `%${searchTerm}%`,
      },
      {
        activityType: anyType,
        extensionId: extensionId,
        pageUrl: `%${searchTerm}%`,
      },
      {
        activityType: anyType,
        extensionId: extensionId,
        argUrl: `%${searchTerm}%`
      }
    ];

    const promises = activityLogFilters.map(
        filter => new Promise(function(resolve, reject) {
          chrome.activityLogPrivate.getExtensionActivities(filter, resolve);
        }));

    return Promise.all(promises).then(results => {
      // We may have results that are present in one or more searches, so
      // we merge them here. We also assume that every distinct activity
      // id corresponds to exactly one activity.
      const activitiesById = new Map();
      for (const result of results) {
        for (const activity of result.activities) {
          activitiesById.set(activity.activityId, activity);
        }
      }

      return {activities: Array.from(activitiesById.values())};
    });
  }

  /** @override */
  deleteActivitiesById(activityIds) {
    return new Promise(function(resolve, reject) {
      chrome.activityLogPrivate.deleteActivities(activityIds, resolve);
    });
  }

  /** @override */
  deleteActivitiesFromExtension(extensionId) {
    return new Promise(function(resolve, reject) {
      chrome.activityLogPrivate.deleteActivitiesByExtension(
          extensionId, resolve);
    });
  }

  /** @override */
  getOnExtensionActivity() {
    return chrome.activityLogPrivate.onExtensionActivity;
  }

  /** @override */
  downloadActivities(rawActivityData, fileName) {
    const blob = new Blob([rawActivityData], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = fileName;
    a.click();
  }

  /**
   * Attempts to load an unpacked extension via a drag-n-drop gesture.
   * @return {!Promise}
   */
  loadUnpackedFromDrag() {
    return this.loadUnpackedHelper_({useDraggedPath: true});
  }

  installDroppedFile() {
    chrome.developerPrivate.installDroppedFile();
  }

  notifyDragInstallInProgress() {
    chrome.developerPrivate.notifyDragInstallInProgress();
  }
}

addSingletonGetter(Service);
