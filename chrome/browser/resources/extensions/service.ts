// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {ActivityLogDelegate} from './activity_log/activity_log_history.js';
import type {ActivityLogEventDelegate} from './activity_log/activity_log_stream.js';
import type {ErrorPageDelegate} from './error_page.js';
import type {ItemDelegate} from './item.js';
import type {KeyboardShortcutDelegate} from './keyboard_shortcut_delegate.js';
import type {LoadErrorDelegate} from './load_error.js';
import type {Mv2DeprecationDelegate} from './mv2_deprecation_delegate.js';
import {Dialog, navigation, Page} from './navigation_helper.js';
import type {PackDialogDelegate} from './pack_dialog.js';
import type {SiteSettingsDelegate} from './site_permissions/site_settings_mixin.js';
import type {ToolbarDelegate} from './toolbar.js';

export interface ServiceInterface extends ActivityLogDelegate,
                                          ActivityLogEventDelegate,
                                          ErrorPageDelegate, ItemDelegate,
                                          KeyboardShortcutDelegate,
                                          LoadErrorDelegate,
                                          Mv2DeprecationDelegate,
                                          PackDialogDelegate,
                                          SiteSettingsDelegate,
                                          ToolbarDelegate {
  notifyDragInstallInProgress(): void;
  loadUnpackedFromDrag(): Promise<boolean>;
  installDroppedFile(): void;
  getProfileStateChangedTarget():
      ChromeEvent<(info: chrome.developerPrivate.ProfileInfo) => void>;
  getProfileConfiguration(): Promise<chrome.developerPrivate.ProfileInfo>;
  getExtensionsInfo(): Promise<chrome.developerPrivate.ExtensionInfo[]>;
  getExtensionSize(id: string): Promise<string>;
  dismissSafetyHubExtensionsMenuNotification(): void;
  dismissMv2DeprecationNotice(): void;
}

export class Service implements ServiceInterface {
  private isDeleting_: boolean = false;
  private eventsToIgnoreOnce_: Set<string> = new Set();

  getProfileConfiguration() {
    return chrome.developerPrivate.getProfileConfiguration();
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

  getExtensionsInfo() {
    return chrome.developerPrivate.getExtensionsInfo(
        {includeDisabled: true, includeTerminated: true});
  }

  getExtensionSize(id: string) {
    return chrome.developerPrivate.getExtensionSize(id);
  }

  addRuntimeHostPermission(id: string, host: string): Promise<void> {
    return chrome.developerPrivate.addHostPermission(id, host);
  }

  removeRuntimeHostPermission(id: string, host: string): Promise<void> {
    return chrome.developerPrivate.removeHostPermission(id, host);
  }

  recordUserAction(metricName: string): void {
    chrome.metricsPrivate.recordUserAction(metricName);
  }

  /**
   * Opens a file browser dialog for the user to select a file (or directory).
   * @return The promise to be resolved with the selected path.
   */
  private chooseFilePath_(
      selectType: chrome.developerPrivate.SelectType,
      fileType: chrome.developerPrivate.FileType): Promise<string> {
    return chrome.developerPrivate.choosePath(selectType, fileType)
        .catch(error => {
          if (error.message !== 'File selection was canceled.') {
            throw error;
          }
          return '';
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
  private loadUnpackedHelper_(extraOptions?:
                                  chrome.developerPrivate.LoadUnpackedOptions):
      Promise<boolean> {
    const options = Object.assign(
        {
          failQuietly: true,
          populateError: true,
        },
        extraOptions);
    return chrome.developerPrivate.loadUnpacked(options)
        .then(loadError => {
          if (loadError) {
            throw loadError;
          }
          // The load was successful if there's no loadError.
          return true;
        })
        .catch(error => {
          if (error.message !== 'File selection was canceled.') {
            throw error;
          }
          return false;
        });
  }

  deleteItem(id: string) {
    if (this.isDeleting_) {
      return;
    }
    chrome.metricsPrivate.recordUserAction('Extensions.RemoveExtensionClick');
    this.isDeleting_ = true;
    chrome.management.uninstall(id, {showConfirmDialog: true})
        .catch(
            _ => {
                // The error was almost certainly the user canceling the dialog.
                // Do nothing. We only check it so we don't get noisy logs.
            })
        .finally(() => {
          this.isDeleting_ = false;
        });
  }

  /**
   * Allows the consumer to call the API asynchronously.
   */
  uninstallItem(id: string): Promise<void> {
    chrome.metricsPrivate.recordUserAction('Extensions.RemoveExtensionClick');
    return chrome.management.uninstall(id, {showConfirmDialog: true});
  }

  deleteItems(ids: string[]): Promise<void> {
    this.isDeleting_ = true;
    return chrome.developerPrivate.removeMultipleExtensions(ids).finally(() => {
      this.isDeleting_ = false;
    });
  }

  setItemSafetyCheckWarningAcknowledged(
      id: string,
      reason: chrome.developerPrivate.SafetyCheckWarningReason): Promise<void> {
    return chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      acknowledgeSafetyCheckWarningReason: reason,
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

  setItemPinnedToToolbar(id: string, pinnedToToolbar: boolean) {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      pinnedToToolbar,
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
    return chrome.developerPrivate
        .reload(id, {failQuietly: true, populateErrorForUnpacked: true})
        .then(loadError => {
          if (loadError) {
            throw loadError;
          }
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

  retryLoadUnpacked(retryGuid?: string): Promise<boolean> {
    // Attempt to load an unpacked extension, optionally as another attempt at
    // a previously-specified load.
    return this.loadUnpackedHelper_({retryGuid});
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

  packExtension(rootPath: string, keyPath: string, flag?: number):
      Promise<chrome.developerPrivate.PackDirectoryResponse> {
    return chrome.developerPrivate.packDirectory(rootPath, keyPath, flag);
  }

  updateAllExtensions(extensions: chrome.developerPrivate.ExtensionInfo[]) {
    /**
     * Attempt to reload local extensions. If an extension fails to load, the
     * user is prompted to try updating the broken extension using loadUnpacked
     * and we skip reloading the remaining local extensions.
     */
    return chrome.developerPrivate.autoUpdate().then(
        () => {
          chrome.metricsPrivate.recordUserAction('Options_UpdateExtensions');
          return new Promise<void>((resolve, reject) => {
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
              resolve();
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
    return chrome.developerPrivate.requestFileSource(args);
  }

  showInFolder(id: string) {
    chrome.developerPrivate.showPath(id);
  }

  getExtensionActivityLog(extensionId: string):
      Promise<chrome.activityLogPrivate.ActivityResultSet> {
    return chrome.activityLogPrivate.getExtensionActivities(
        {
          activityType: chrome.activityLogPrivate.ExtensionActivityFilter.ANY,
          extensionId: extensionId,
        },
    );
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
        argUrl: `%${searchTerm}%`,
      },
    ];

    const promises:
        Array<Promise<chrome.activityLogPrivate.ActivityResultSet>> =
            activityLogFilters.map(
                filter =>
                    chrome.activityLogPrivate.getExtensionActivities(filter));

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
    return chrome.activityLogPrivate.deleteActivities(activityIds);
  }

  deleteActivitiesFromExtension(extensionId: string): Promise<void> {
    return chrome.activityLogPrivate.deleteActivitiesByExtension(extensionId);
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

  getUserSiteSettings(): Promise<chrome.developerPrivate.UserSiteSettings> {
    return chrome.developerPrivate.getUserSiteSettings();
  }

  addUserSpecifiedSites(
      siteSet: chrome.developerPrivate.SiteSet,
      hosts: string[]): Promise<void> {
    return chrome.developerPrivate.addUserSpecifiedSites({siteSet, hosts});
  }

  removeUserSpecifiedSites(
      siteSet: chrome.developerPrivate.SiteSet,
      hosts: string[]): Promise<void> {
    return chrome.developerPrivate.removeUserSpecifiedSites({siteSet, hosts});
  }

  getUserAndExtensionSitesByEtld():
      Promise<chrome.developerPrivate.SiteGroup[]> {
    return chrome.developerPrivate.getUserAndExtensionSitesByEtld();
  }

  getMatchingExtensionsForSite(site: string):
      Promise<chrome.developerPrivate.MatchingExtensionInfo[]> {
    return chrome.developerPrivate.getMatchingExtensionsForSite(site);
  }

  getUserSiteSettingsChangedTarget() {
    return chrome.developerPrivate.onUserSiteSettingsChanged;
  }

  setShowAccessRequestsInToolbar(id: string, showRequests: boolean) {
    chrome.developerPrivate.updateExtensionConfiguration({
      extensionId: id,
      showAccessRequestsInToolbar: showRequests,
    });
  }

  updateSiteAccess(
      site: string,
      updates: chrome.developerPrivate.ExtensionSiteAccessUpdate[]):
      Promise<void> {
    return chrome.developerPrivate.updateSiteAccess(site, updates);
  }

  dismissSafetyHubExtensionsMenuNotification() {
    chrome.developerPrivate.dismissSafetyHubExtensionsMenuNotification();
  }

  dismissMv2DeprecationNotice(): void {
    chrome.developerPrivate.updateProfileConfiguration(
        {isMv2DeprecationNoticeDismissed: true});
  }

  dismissMv2DeprecationNoticeForExtension(id: string): Promise<void> {
    return chrome.developerPrivate.dismissMv2DeprecationNoticeForExtension(id);
  }

  static getInstance(): ServiceInterface {
    return instance || (instance = new Service());
  }

  static setInstance(obj: ServiceInterface) {
    instance = obj;
  }
}

let instance: ServiceInterface|null = null;
