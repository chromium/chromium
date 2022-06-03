// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {assert} from 'chrome://resources/js/assert.m.js';

import {ActivityLogDelegate} from './activity_log/activity_log_history.js';
import {ActivityLogEventDelegate} from './activity_log/activity_log_stream.js';
import {ErrorPageDelegate} from './error_page.js';
import {ItemDelegate} from './item.js';
import {KeyboardShortcutDelegate} from './keyboard_shortcut_delegate.js';
import {LoadErrorDelegate} from './load_error.js';
import {Dialog, navigation, Page} from './navigation_helper.js';
import {PackDialogDelegate} from './pack_dialog.js';
import {ToolbarDelegate} from './toolbar.js';

export class Service implements ActivityLogDelegate, ActivityLogEventDelegate,
                                ErrorPageDelegate, ItemDelegate,
                                KeyboardShortcutDelegate, LoadErrorDelegate,
                                PackDialogDelegate, ToolbarDelegate {
  private isDeleting_: boolean = false;
  private eventsToIgnoreOnce_: Set<string> = new Set();

  getProfileConfiguration(): Promise<chrome.developerPrivate.ProfileInfo> {
    return new Promise(function(resolve) {
      chrome.developerPrivate.getProfileConfiguration(resolve);
    });
  }

  getItemStateChangedTarget() {
    return chrome.developerPrivate.onItemStateChanged;
  }

  shouldIgnoreUpdate(
      extensionId: string,
      eventType: chrome.developerPrivate.EventType): boolean {
    return this.eventsToIgnoreOnce_.delete(`${extensionId}_${eventType}`);
  }

  ignoreNextEvent(
      extensionId: string, eventType: chrome.developerPrivate.EventType): void {
    this.eventsToIgnoreOnce_.add(`${extensionId}_${eventType}`);
  }

  getProfileStateChangedTarget() {
    return chrome.developerPrivate.onProfileStateChanged;
  }

  getExtensionsInfo(): Promise<Array<chrome.developerPrivate.ExtensionInfo>> {
    return new Promise(function(resolve) {
      chrome.developerPrivate.getExtensionsInfo(
          {includeDisabled: true, includeTerminated: true}, resolve);
    });
  }

  getExtensionSize(id: string): Promise<string> {
    return new Promise(function(resolve) {
      chrome.developerPrivate.getExtensionSize(id, resolve);
    });
  }

  addRuntimeHostPermission(id: string, host: string): Promise<void> {
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

  removeRuntimeHostPermission(id: string, host: string): Promise<void> {
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

  recordUserAction(metricName: string): void {
    chrome.metricsPrivate.recordUserAction(metricName);
  }

  /**
   * Opens a file browser dialog for the user to select a file (or directory).
   * @return The promise to be resolved with the selected path.
   */
  chooseFilePath_(
      selectType: chrome.developerPrivate.SelectType,
      fileType: chrome.developerPrivate.FileType): Promise<string> {
    return new Promise(function(resolve, reject) {
      chrome.developerPrivate.choosePath(selectType, fileType, function(path) {
        if (chrome.runtime.lastError &&
            chrome.runtime.lastError.message !==
                'File selection was canceled.') {
          reject(chrome.runtime.lastError);
        } else {
          resolve(path || '');
        }
      });
    });
  }

  updateExtensionCommandKeybinding(
      extensionId: string, commandName: string, keybinding: string) {
    chrome.developerPrivate.updateExtensionCommand({
      extensionId: extensionId,
      commandName: commandName,
      keybinding: keybinding,
    });
  }

  updateExtensionCommandScope(
      extensionId: string, commandName: string,
      scope: chrome.developerPrivate.CommandScope): void {
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


  setShortcutHandlingSuspended(isCapturing: boolean) {
    chrome.developerPrivate.setShortcutHandlingSuspended(isCapturing);
  }

  /**
   * @return A signal that loading finished, rejected if any error occurred.
   */
  private loadUnpackedHelper_(opt_options?:
                                  chrome.developerPrivate.LoadUnpackedOptions):
      Promise<boolean> {
    return new Promise(function(resolve, reject) {
      const options = Object.assign(
          {
            failQuietly: true,
            populateError: true,
          },
          opt_options);

      chrome.developerPrivate.loadUnpacked(options, (loadError) => {
        if (chrome.runtime.lastError &&
            chrome.runtime.lastError.message !==
                'File selection was canceled.') {
          throw new Error(chrome.runtime.lastError.message);
        }
        if (loadError) {
          return reject(loadError);
        }
        // The load was successful if there's no lastError indicated (and
        // no loadError, which is checked above).
        const loadSuccessful = typeof chrome.runtime.lastError === 'undefined';
        resolve(loadSuccessful);
      });
    });
  }

  deleteItem(id: string) {
    if (this.isDeleting_) {
      return;
    }
    chrome.metricsPrivate.recordUserAction('Extensions.RemoveExtensionClick');
    this.isDeleting_ = true;
    chrome.management.uninstall(id, {showConfirmDialog: true}, () => {
      // The "last error" was almost certainly the user canceling the dialog.
      // Do nothing. We only check it so we don't get noisy logs.
      /** @suppress {suspiciousCode} */
      chrome.runtime.lastError;
      this.isDeleting_ = false;
    });
  }

  setItemEnabled(id: string, isEnabled: boolean) {
    chrome.metricsPrivate.recordUserAction(
        isEnabled ? 'Extensions.ExtensionEnabled' :
                    'Extensions.ExtensionDisabled');
    chrome.management.setEnabled(id, isEnabled);
  }

  setItemAllowedIncognito(id: string, isAllowedIncognito: boolean) {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      incognitoAccess: isAllowedIncognito,
    });
  }

  setItemAllowedOnFileUrls(id: string, isAllowedOnFileUrls: boolean) {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      fileAccess: isAllowedOnFileUrls,
    });
  }

  setItemHostAccess(id: string, hostAccess: chrome.developerPrivate.HostAccess):
      void {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      hostAccess: hostAccess,
    });
  }

  setItemCollectsErrors(id: string, collectsErrors: boolean): void {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      errorCollection: collectsErrors,
    });
  }

  inspectItemView(id: string, view: chrome.developerPrivate.ExtensionView):
      void {
    chrome.developerPrivate.openDevTools({
      extensionId: id,
      renderProcessId: view.renderProcessId,
      renderViewId: view.renderViewId,
      incognito: view.incognito,
      isServiceWorker: view.type === 'EXTENSION_SERVICE_WORKER_BACKGROUND',
    });
  }

  openUrl(url: string): void {
    window.open(url);
  }

  reloadItem(id: string): Promise<void> {
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

  repairItem(id: string): void {
    chrome.developerPrivate.repairExtension(id);
  }

  showItemOptionsPage(extension: chrome.developerPrivate.ExtensionInfo): void {
    assert(extension && extension.optionsPage);
    if (extension.optionsPage!.openInTab) {
      chrome.developerPrivate.showOptions(extension.id);
    } else {
      navigation.navigateTo({
        page: Page.DETAILS,
        subpage: Dialog.OPTIONS,
        extensionId: extension.id,
      });
    }
  }

  setProfileInDevMode(inDevMode: boolean) {
    chrome.developerPrivate.updateProfileConfiguration(
        {inDeveloperMode: inDevMode});
  }

  loadUnpacked(): Promise<boolean> {
    return this.loadUnpackedHelper_();
  }

  retryLoadUnpacked(retryGuid: string): Promise<boolean> {
    // Attempt to load an unpacked extension, optionally as another attempt at
    // a previously-specified load.
    return this.loadUnpackedHelper_({retryGuid: retryGuid});
  }

  choosePackRootDirectory(): Promise<string> {
    return this.chooseFilePath_(
        chrome.developerPrivate.SelectType.FOLDER,
        chrome.developerPrivate.FileType.LOAD);
  }

  choosePrivateKeyPath(): Promise<string> {
    return this.chooseFilePath_(
        chrome.developerPrivate.SelectType.FILE,
        chrome.developerPrivate.FileType.PEM);
  }

  packExtension(
      rootPath: string, keyPath: string, flag?: number,
      callback?:
          (response: chrome.developerPrivate.PackDirectoryResponse) => void):
      void {
    chrome.developerPrivate.packDirectory(rootPath, keyPath, flag, callback);
  }

  updateAllExtensions(extensions: chrome.developerPrivate.ExtensionInfo[]):
      Promise<string> {
    /**
     * Attempt to reload local extensions. If an extension fails to load, the
     * user is prompted to try updating the broken extension using loadUnpacked
     * and we skip reloading the remaining local extensions.
     */
    return new Promise<void>((resolve) => {
             chrome.developerPrivate.autoUpdate(() => resolve());
             chrome.metricsPrivate.recordUserAction('Options_UpdateExtensions');
           })
        .then(() => {
          return new Promise((resolve, reject) => {
            const loadLocalExtensions = async () => {
              for (const extension of extensions) {
                if (extension.location === 'UNPACKED') {
                  try {
                    await this.reloadItem(extension.id);
                  } catch (loadError) {
                    reject(loadError);
                    break;
                  }
                }
              }
              resolve('Loaded local extensions.');
            };
            loadLocalExtensions();
          });
        });
  }

  deleteErrors(
      extensionId: string, errorIds?: number[],
      type?: chrome.developerPrivate.ErrorType) {
    chrome.developerPrivate.deleteExtensionErrors({
      extensionId: extensionId,
      errorIds: errorIds,
      type: type,
    });
  }

  requestFileSource(args: chrome.developerPrivate.RequestFileSourceProperties):
      Promise<chrome.developerPrivate.RequestFileSourceResponse> {
    return new Promise(function(resolve) {
      chrome.developerPrivate.requestFileSource(args, resolve);
    });
  }

  showInFolder(id: string) {
    chrome.developerPrivate.showPath(id);
  }

  getExtensionActivityLog(extensionId: string):
      Promise<chrome.activityLogPrivate.ActivityResultSet> {
    return new Promise(function(resolve) {
      chrome.activityLogPrivate.getExtensionActivities(
          {
            activityType: chrome.activityLogPrivate.ExtensionActivityFilter.ANY,
            extensionId: extensionId
          },
          resolve);
    });
  }

  getFilteredExtensionActivityLog(extensionId: string, searchTerm: string) {
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

    const promises:
        Array<Promise<chrome.activityLogPrivate.ActivityResultSet>> =
            activityLogFilters.map(
                filter => new Promise(function(resolve) {
                  chrome.activityLogPrivate.getExtensionActivities(
                      filter, resolve);
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

  deleteActivitiesById(activityIds: string[]): Promise<void> {
    return new Promise(function(resolve) {
      chrome.activityLogPrivate.deleteActivities(activityIds, resolve);
    });
  }

  deleteActivitiesFromExtension(extensionId: string): Promise<void> {
    return new Promise(function(resolve) {
      chrome.activityLogPrivate.deleteActivitiesByExtension(
          extensionId, resolve);
    });
  }

  getOnExtensionActivity(): ChromeEvent<
      (activity: chrome.activityLogPrivate.ExtensionActivity) => void> {
    return chrome.activityLogPrivate.onExtensionActivity;
  }

  downloadActivities(rawActivityData: string, fileName: string) {
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

  static getInstance(): Service {
    return instance || (instance = new Service());
  }

  static setInstance(obj: Service) {
    instance = obj;
  }
}

let instance: Service|null = null;
