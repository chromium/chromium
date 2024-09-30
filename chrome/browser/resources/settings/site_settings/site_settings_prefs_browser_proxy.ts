// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Site Settings" section to
 * interact with the content settings prefs.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {ChooserType,ContentSetting,ContentSettingsTypes,SiteSettingSource} from './constants.js';
// clang-format on

/**
 * The handler will send a policy source that is similar, but not exactly the
 * same as a ControlledBy value. If the DefaultSettingSource is omitted it
 * should be treated as 'default'.
 * Should be kept in sync with values returned by C++ function
 * `ProviderToDefaultSettingSourceString`.
 */
export enum DefaultSettingSource {
  POLICY = 'policy',
  SUPERVISED_USER = 'supervised_user',
  EXTENSION = 'extension',
  PREFERENCE = 'preference',
  DEFAULT = 'default',
}

/**
 * Stores information about if a content setting is valid, and why.
 */
interface IsValid {
  isValid: boolean;
  reason: string|null;
}

/**
 * Stores origin information. The |hasPermissionSettings| will be set to true
 * when this origin has permissions or when there is a pattern permission
 * affecting this origin.
 */
export interface OriginInfo {
  origin: string;
  engagement: number;
  usage: number;
  numCookies: number;
  hasPermissionSettings: boolean;
  isInstalled: boolean;
  isPartitioned: boolean;
}

/**
 * Represents a list of related sites, grouped by 'groupingKey', which will be
 * an eTLD+1 for HTTP(S) sites, or an origin for other schemes. 'groupingKey'
 * will be unique for each SiteGroup, but should be treated as an opaque token
 * in UI code.
 */
export interface SiteGroup {
  groupingKey: string;
  displayName: string;
  numCookies: number;
  origins: OriginInfo[];
  etldPlus1?: string;
  rwsOwner?: string;
  rwsNumMembers?: number;
  rwsEnterpriseManaged?: boolean;
  hasInstalledPWA: boolean;
}

/**
 * The site exception information passed from the C++ handler.
 * See also: SiteException.
 */
export interface RawSiteException {
  embeddingOrigin: string;
  incognito: boolean;
  isEmbargoed: boolean;
  origin: string;
  displayName: string;
  type: string;
  description?: string;
  setting: ContentSetting;
  source: SiteSettingSource;
}

/**
 * The site exception after it has been converted/filtered for UI use.
 * See also: RawSiteException.
 */
export interface SiteException {
  category: ContentSettingsTypes;
  embeddingOrigin: string;
  incognito: boolean;
  isEmbargoed: boolean;
  origin: string;
  displayName: string;
  setting: ContentSetting;
  description?: string;
  enforcement: chrome.settingsPrivate.Enforcement|null;
  controlledBy: chrome.settingsPrivate.ControlledBy;
}

/**
 * A group of storage access site exceptions with the same origin for UI use.
 * See also: StorageAccessEmbeddingException.
 */
export interface StorageAccessSiteException {
  origin: string;
  displayName: string;
  setting: ContentSetting;

  // Information needed for a static row.
  description?: string;
  incognito?: boolean;

  // Information needed for a grouped row.
  closeDescription?: string;
  openDescription?: string;

  exceptions: StorageAccessEmbeddingException[];
}

/**
 * A storage access site exception for UI use. To be always used within
 * StorageAccessSiteException.
 */
export interface StorageAccessEmbeddingException {
  embeddingOrigin: string;
  embeddingDisplayName: string;
  description?: string;  // includes case for embargoed exception.
  incognito: boolean;
}

/**
 * Represents a list of exceptions recently configured for a site, where recent
 * is defined by the maximum number of sources parameter passed to
 * GetRecentSitePermissions.
 */
export interface RecentSitePermissions {
  origin: string;
  displayName: string;
  incognito: boolean;
  recentPermissions: RawSiteException[];
}

/**
 * The chooser exception information passed from the C++ handler.
 * See also: ChooserException.
 */
export interface RawChooserException {
  chooserType: ChooserType;
  displayName: string;
  object: Object;
  sites: RawSiteException[];
}

/**
 * The chooser exception after it has been converted/filtered for UI use.
 * See also: RawChooserException.
 */
export interface ChooserException {
  chooserType: ChooserType;
  displayName: string;
  object: Object;
  sites: SiteException[];
}

export interface DefaultContentSetting {
  setting: ContentSetting;
  source: DefaultSettingSource;
}

export interface MediaPickerEntry {
  name: string;
  id: string;
}

export interface ZoomLevelEntry {
  displayName: string;
  hostOrSpec: string;
  originForFavicon: string;
  zoom: string;
}

export interface FileSystemGrant {
  filePath: string;
  displayName: string;
  isDirectory: boolean;
}

export interface OriginFileSystemGrants {
  origin: string;
  viewGrants: FileSystemGrant[];
  editGrants: FileSystemGrant[];
}

export interface SmartCardReaderGrants {
  readerName: string;
  origins: string[];
}

export interface SiteSettingsPrefsBrowserProxy {
  /**
   * Sets the default value for a site settings category.
   * @param contentType The name of the category to change.
   * @param defaultValue The name of the value to set as default.
   */
  setDefaultValueForContentType(contentType: string, defaultValue: string):
      void;

  /**
   * Gets the default value for a site settings category.
   */
  getDefaultValueForContentType(contentType: ContentSettingsTypes):
      Promise<DefaultContentSetting>;

  /**
   * Gets a list of sites, grouped by eTLD+1, affected by any content settings
   * that should be visible to the user.
   */
  getAllSites(): Promise<SiteGroup[]>;

  /**
   * Returns a list of content settings types that are controlled via a standard
   * permissions UI and should be made visible to the user.
   * @param origin The associated origin for which categories should be shown or
   *     hidden.
   */
  getCategoryList(origin: string): Promise<ContentSettingsTypes[]>;

  /**
   * Gets most recently changed permissions grouped by host and limited to
   * numSources different origin/profile (inconigto/regular) pairings.
   * This includes permissions adjusted by embargo, but excludes any set
   * via policy.
   * @param numSources Maximum number of different sources to return
   */
  getRecentSitePermissions(numSources: number):
      Promise<RecentSitePermissions[]>;

  /**
   * Gets the chooser exceptions for a particular chooser type.
   * @param chooserType The chooser type to grab exceptions from.
   */
  getChooserExceptionList(chooserType: ChooserType):
      Promise<RawChooserException[]>;

  /**
   * Converts a given number of bytes into a human-readable format, with data
   * units.
   */
  getFormattedBytes(numBytes: number): Promise<string>;

  /**
   * Gets the exceptions (site list) for a particular category.
   * @param contentType The name of the category to query.
   */
  getExceptionList(contentType: ContentSettingsTypes):
      Promise<RawSiteException[]>;

  getStorageAccessExceptionList(categorySubtype: ContentSetting):
      Promise<StorageAccessSiteException[]>;

  /**
   * Gets the File System Access permission grants, grouped by origin.
   */
  getFileSystemGrants(): Promise<OriginFileSystemGrants[]>;

  revokeFileSystemGrant(origin: string, filePath: string): void;

  revokeFileSystemGrants(origin: string): void;

  /**
   * Gets the persistent Smart Card Reader permission grants, grouped by reader
   * name.
   */
  getSmartCardReaderGrants(): Promise<SmartCardReaderGrants[]>;

  /**
   * Revokes all Smart Card Reader permission grants.
   */
  revokeAllSmartCardReadersGrants(): void;

  /**
   * Revokes a particular persistent Smart Card Reader permission grant.
   * @param reader The smart card reader name.
   * @param origin URL of the site that was granted the permission.
   */
  revokeSmartCardReaderGrant(reader: string, origin: string): void;

  /**
   * Gets a list of category permissions for a given origin. Note that this
   * may be different to the results retrieved by getExceptionList(), since it
   * combines different sources of data to get a permission's value.
   * @param origin The origin to look up permissions for.
   * @param contentTypes A list of categories to retrieve the ContentSetting
   *     for.
   */
  getOriginPermissions(origin: string, contentTypes: ContentSettingsTypes[]):
      Promise<RawSiteException[]>;

  /**
   * Resets the permissions for a list of categories for a given origin. This
   * does not support incognito settings or patterns.
   * @param origin The origin to reset permissions for.
   * @param category to set the permission for. If null, this applies to all
   *     categories. (Sometimes it is useful to clear any permissions set for
   *     all categories.)
   * @param blanketSetting The setting to set all permissions listed in
   *     |contentTypes| to.
   */
  setOriginPermissions(
      origin: string, category: ContentSettingsTypes|null,
      blanketSetting: ContentSetting): void;

  /**
   * Resets the category permission for a given origin (expressed as primary
   * and secondary patterns). Only use this if intending to remove an
   * exception - use setOriginPermissions() for origin-scoped settings.
   * @param primaryPattern The origin to change (primary pattern).
   * @param secondaryPattern The embedding origin to change (secondary
   *     pattern).
   * @param contentType The name of the category to reset.
   * @param incognito Whether this applies only to a current
   *     incognito session exception.
   */
  resetCategoryPermissionForPattern(
      primaryPattern: string, secondaryPattern: string, contentType: string,
      incognito: boolean): void;

  /**
   * Removes a particular chooser object permission by origin and embedding
   * origin.
   * @param chooserType The chooser exception type
   * @param origin The origin to look up the permission for.
   * @param exception The exception to revoke permission for.
   */
  resetChooserExceptionForSite(
      chooserType: ChooserType, origin: string, exception: Object): void;

  /**
   * Sets the category permission for a given origin (expressed as primary and
   * secondary patterns). Only use this if intending to set an exception - use
   * setOriginPermissions() for origin-scoped settings.
   * @param primaryPattern The origin to change (primary pattern).
   * @param secondaryPattern The embedding origin to change (secondary pattern).
   * @param contentType The name of the category to change.
   * @param value The value to change the permission to.
   * @param incognito Whether this rule applies only to the current incognito
   *     session.
   */
  setCategoryPermissionForPattern(
      primaryPattern: string, secondaryPattern: string, contentType: string,
      value: string, incognito: boolean): void;

  /**
   * Checks whether an origin is valid.
   */
  isOriginValid(origin: string): Promise<boolean>;

  /**
   * Checks whether a setting is valid.
   * @param pattern The pattern to check.
   * @param category What kind of setting, e.g. Location, Camera, Cookies, etc.
   * @return Contains whether or not the pattern is valid for the type, and if
   *     it is invalid, the reason why.
   */
  isPatternValidForType(pattern: string, category: ContentSettingsTypes):
      Promise<IsValid>;

  /**
   * Requests initialization of the capture device list. The list is returned
   * through a JS call to updateDevicesMenu.
   * @param type The type to look up.
   */
  initializeCaptureDevices(type: string): void;

  /**
   * Sets a preferred device for the given type of media.
   * @param type The type of media to configure.
   * @param defaultValue The id of the media device to set.
   */
  setPreferredCaptureDevice(type: string, defaultValue: string): void;

  /**
   * observes _all_ of the the protocol handler state, which includes a list
   * that is returned through JS calls to 'setProtocolHandlers' along with
   * other state sent with the messages 'setIgnoredProtocolHandler' and
   * 'setHandlersEnabled'.
   */
  observeProtocolHandlers(): void;

  /**
   * observes _all_ of the the app protocol handler state, which includes a
   * list that is returned through 'setAppAllowedProtocolHandlers' events.
   */
  observeAppProtocolHandlers(): void;

  /**
   * Observes one aspect of the protocol handler so that updates to the
   * enabled/disabled state are sent. A 'setHandlersEnabled' will be sent
   * from C++ immediately after receiving this observe request and updates
   * may follow via additional 'setHandlersEnabled' messages.
   *
   * If |observeProtocolHandlers| is called, there's no need to call this
   * observe as well.
   */
  observeProtocolHandlersEnabledState(): void;

  /**
   * Enables or disables the ability for sites to ask to become the default
   * protocol handlers.
   * @param enabled Whether sites can ask to become default.
   */
  setProtocolHandlerDefault(enabled: boolean): void;

  /**
   * Sets a certain url as default for a given protocol handler.
   * @param protocol The protocol to set a default for.
   * @param url The url to use as the default.
   */
  setProtocolDefault(protocol: string, url: string): void;

  /**
   * Deletes a certain protocol handler by url.
   * @param protocol The protocol to delete the url from.
   * @param url The url to delete.
   */
  removeProtocolHandler(protocol: string, url: string): void;

  /**
   * Deletes a protocol handler by url from the app allowed list.
   * @param protocol The protocol to delete the url from.
   * @param url The url to delete.
   * @param appId The web app's ID to delete.
   */
  removeAppAllowedHandler(protocol: string, url: string, appId: string): void;

  /**
   * Deletes a protocol handler by url from the app approved list.
   * @param protocol The protocol to delete the url from.
   * @param url The url to delete.
   * @param appId The web app's ID to delete.
   */
  removeAppDisallowedHandler(protocol: string, url: string, appId: string):
      void;

  /**
   * Fetches the incognito status of the current profile (whether an incognito
   * profile exists). Returns the results via onIncognitoStatusChanged.
   */
  updateIncognitoStatus(): void;

  /**
   * Fetches the currently defined zoom levels for sites. Returns the results
   * via onZoomLevelsChanged.
   */
  fetchZoomLevels(): void;

  /**
   * Removes a zoom levels for a given host.
   * @param host The host to remove zoom levels for.
   */
  removeZoomLevel(host: string): void;

  /**
   * Fetches the current block autoplay state. Returns the results via
   * onBlockAutoplayStatusChanged.
   */
  fetchBlockAutoplayStatus(): void;

  /**
   * Clears all the web storage data and cookies for a given site group.
   * @param groupingKey The group to clear data from.
   */
  clearSiteGroupDataAndCookies(groupingKey: string): void;

  /**
   * Clears all the unpartitioned web storage data and cookies for a given
   * origin.
   * @param origin The origin to clear data from.
   */
  clearUnpartitionedOriginDataAndCookies(origin: string): void;

  /**
   * Clears all the storage for |origin| which is partitioned on |groupingKey|.
   * @param origin The origin to clear data from.
   * @param groupingKey The groupingKey which the data is partitioned for.
   */
  clearPartitionedOriginDataAndCookies(origin: string, groupingKey: string):
      void;

  /**
   * Record All Sites Page action for metrics.
   * @param action number.
   */
  recordAction(action: number): void;

  /**
   * Gets display string for RWS information of owner and member count.
   * @param rwsNumMembers The number of members in the related website set.
   * @param rwsOwner The eTLD+1 for the related website set owner.
   */
  getRwsMembershipLabel(rwsNumMembers: number, rwsOwner: string):
      Promise<string>;

  /**
   * Gets the plural string for a given number of cookies.
   * @param numCookies The number of cookies.
   */
  getNumCookiesString(numCookies: number): Promise<string>;

  /**
   * Gets the warning messages for the permissions types blocked at the OS
   * level.
   */
  getSystemDeniedPermissions(): Promise<ContentSettingsTypes[]>;

  /**
   * Attempts to open a system setting page that allows to modify the system
   * wide block for a given permission type. If this is not possible, the call
   * will fail silently and no window will open.
   * @param contentType The permission type.
   */
  openSystemPermissionSettings(contentType: string): void;
}

export class SiteSettingsPrefsBrowserProxyImpl implements
    SiteSettingsPrefsBrowserProxy {
  setDefaultValueForContentType(contentType: string, defaultValue: string) {
    chrome.send('setDefaultValueForContentType', [contentType, defaultValue]);
  }

  getDefaultValueForContentType(contentType: ContentSettingsTypes) {
    return sendWithPromise('getDefaultValueForContentType', contentType);
  }

  getAllSites() {
    return sendWithPromise('getAllSites');
  }

  getCategoryList(origin: string) {
    return sendWithPromise('getCategoryList', origin);
  }

  getRecentSitePermissions(numSources: number) {
    return sendWithPromise('getRecentSitePermissions', numSources);
  }

  getChooserExceptionList(chooserType: ChooserType) {
    return sendWithPromise('getChooserExceptionList', chooserType);
  }

  getFormattedBytes(numBytes: number) {
    return sendWithPromise('getFormattedBytes', numBytes);
  }

  getExceptionList(contentType: ContentSettingsTypes) {
    return sendWithPromise('getExceptionList', contentType);
  }

  getStorageAccessExceptionList(categorySubtype: ContentSetting) {
    return sendWithPromise('getStorageAccessExceptionList', categorySubtype);
  }

  getFileSystemGrants() {
    return sendWithPromise('getFileSystemGrants');
  }

  revokeFileSystemGrant(origin: string, filePath: string) {
    chrome.send('revokeFileSystemGrant', [origin, filePath]);
  }

  revokeFileSystemGrants(origin: string) {
    chrome.send('revokeFileSystemGrants', [origin]);
  }

  getSmartCardReaderGrants() {
    return sendWithPromise('getSmartCardReaderGrants');
  }

  revokeAllSmartCardReadersGrants() {
    chrome.send('revokeAllSmartCardReadersGrants');
  }

  revokeSmartCardReaderGrant(reader: string, origin: string) {
    chrome.send('revokeSmartCardReaderGrant', [reader, origin]);
  }

  getOriginPermissions(origin: string, contentTypes: ContentSettingsTypes[]) {
    return sendWithPromise('getOriginPermissions', origin, contentTypes);
  }

  setOriginPermissions(
      origin: string, category: ContentSettingsTypes|null,
      blanketSetting: ContentSetting) {
    chrome.send('setOriginPermissions', [origin, category, blanketSetting]);
  }

  resetCategoryPermissionForPattern(
      primaryPattern: string, secondaryPattern: string, contentType: string,
      incognito: boolean) {
    chrome.send(
        'resetCategoryPermissionForPattern',
        [primaryPattern, secondaryPattern, contentType, incognito]);
  }

  resetChooserExceptionForSite(
      chooserType: ChooserType, origin: string, exception: Object) {
    chrome.send(
        'resetChooserExceptionForSite', [chooserType, origin, exception]);
  }

  setCategoryPermissionForPattern(
      primaryPattern: string, secondaryPattern: string, contentType: string,
      value: string, incognito: boolean) {
    chrome.send(
        'setCategoryPermissionForPattern',
        [primaryPattern, secondaryPattern, contentType, value, incognito]);
  }

  isOriginValid(origin: string) {
    return sendWithPromise('isOriginValid', origin);
  }

  isPatternValidForType(pattern: string, category: ContentSettingsTypes) {
    return sendWithPromise('isPatternValidForType', pattern, category);
  }

  initializeCaptureDevices(type: string) {
    chrome.send('initializeCaptureDevices', [type]);
  }

  setPreferredCaptureDevice(type: string, defaultValue: string) {
    chrome.send('setPreferredCaptureDevice', [type, defaultValue]);
  }

  observeProtocolHandlers() {
    chrome.send('observeProtocolHandlers');
  }

  observeAppProtocolHandlers() {
    chrome.send('observeAppProtocolHandlers');
  }

  observeProtocolHandlersEnabledState() {
    chrome.send('observeProtocolHandlersEnabledState');
  }

  setProtocolHandlerDefault(enabled: boolean) {
    chrome.send('setHandlersEnabled', [enabled]);
  }

  setProtocolDefault(protocol: string, url: string) {
    chrome.send('setDefault', [protocol, url]);
  }

  removeProtocolHandler(protocol: string, url: string) {
    chrome.send('removeHandler', [protocol, url]);
  }

  removeAppAllowedHandler(protocol: string, url: string, appId: string) {
    chrome.send('removeAppAllowedHandler', [protocol, url, appId]);
  }

  removeAppDisallowedHandler(protocol: string, url: string, appId: string) {
    chrome.send('removeAppDisallowedHandler', [protocol, url, appId]);
  }

  updateIncognitoStatus() {
    chrome.send('updateIncognitoStatus');
  }

  fetchZoomLevels() {
    chrome.send('fetchZoomLevels');
  }

  removeZoomLevel(host: string) {
    chrome.send('removeZoomLevel', [host]);
  }

  fetchBlockAutoplayStatus() {
    chrome.send('fetchBlockAutoplayStatus');
  }

  clearSiteGroupDataAndCookies(groupingKey: string) {
    chrome.send('clearSiteGroupDataAndCookies', [groupingKey]);
  }

  clearUnpartitionedOriginDataAndCookies(origin: string) {
    chrome.send('clearUnpartitionedUsage', [origin]);
  }

  clearPartitionedOriginDataAndCookies(origin: string, groupingKey: string) {
    chrome.send('clearPartitionedUsage', [origin, groupingKey]);
  }

  recordAction(action: number) {
    chrome.send('recordAction', [action]);
  }

  getRwsMembershipLabel(rwsNumMembers: number, rwsOwner: string) {
    return sendWithPromise('getRwsMembershipLabel', rwsNumMembers, rwsOwner);
  }

  getNumCookiesString(numCookies: number) {
    return sendWithPromise('getNumCookiesString', numCookies);
  }

  getSystemDeniedPermissions() {
    return sendWithPromise('getSystemDeniedPermissions');
  }

  openSystemPermissionSettings(contentType: string) {
    chrome.send('openSystemPermissionSettings', [contentType]);
  }

  static getInstance(): SiteSettingsPrefsBrowserProxy {
    return instance || (instance = new SiteSettingsPrefsBrowserProxyImpl());
  }

  static setInstance(obj: SiteSettingsPrefsBrowserProxy) {
    instance = obj;
  }
}

let instance: SiteSettingsPrefsBrowserProxy|null = null;
