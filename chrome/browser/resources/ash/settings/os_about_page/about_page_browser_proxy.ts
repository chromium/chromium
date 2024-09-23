// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "About" section to interact with
 * the browser.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface RegulatoryInfo {
  text: string;
  url: string;
}

/**
 * Enumeration of all possible browser channels.
 */
export enum BrowserChannel {
  BETA = 'beta-channel',
  CANARY = 'canary-channel',
  DEV = 'dev-channel',
  STABLE = 'stable-channel',
  LTC = 'ltc-channel',
  LTS = 'lts-channel',
}

export interface ChannelInfo {
  currentChannel: BrowserChannel;
  targetChannel: BrowserChannel;
  isLts: boolean;
}

export interface VersionInfo {
  arcVersion: string;
  osFirmware: string;
  osVersion: string;
}

export interface AboutPageUpdateInfo {
  version?: string;
  size?: string;
}

/**
 * Information related to device end of life. These values will always be
 * populated by the C++ handler.
 */
export interface EndOfLifeInfo {
  hasEndOfLife: boolean;
  aboutPageEndOfLifeMessage: string;
  shouldShowEndOfLifeIncentive: boolean;
  shouldShowOfferText: boolean;
  isExtendedUpdatesDatePassed: boolean;
  isExtendedUpdatesOptInRequired: boolean;
}

export interface TpmFirmwareUpdateStatusChangedEvent {
  updateAvailable: boolean;
}

/**
 * Enumeration of all possible update statuses. The string literals must match
 * the ones defined at |AboutHandler::UpdateStatusToString|.
 */
export enum UpdateStatus {
  CHECKING = 'checking',
  UPDATING = 'updating',
  NEARLY_UPDATED = 'nearly_updated',
  UPDATED = 'updated',
  FAILED = 'failed',
  FAILED_HTTP = 'failed_http',
  FAILED_DOWNLOAD = 'failed_download',
  DISABLED = 'disabled',
  UPDATE_TO_ROLLBACK_VERSION_DISALLOWED =
      'update_to_rollback_version_disallowed',
  DISABLED_BY_ADMIN = 'disabled_by_admin',
  NEED_PERMISSION_TO_UPDATE = 'need_permission_to_update',
  DEFERRED = 'deferred',
}

export interface UpdateStatusChangedEvent {
  status: UpdateStatus;
  progress?: number;
  message?: string;
  connectionTypes?: string;
  version?: string;
  size?: string;
  rollback?: boolean;
  powerwash?: boolean;
}

export function browserChannelToI18nId(
    channel: BrowserChannel, isLts: boolean): string {
  // TODO(b/273717293): Remove LTS tag handling.
  if (isLts) {
    return 'aboutChannelLongTermSupport';
  }

  switch (channel) {
    case BrowserChannel.BETA:
      return 'aboutChannelBeta';
    case BrowserChannel.CANARY:
      return 'aboutChannelCanary';
    case BrowserChannel.DEV:
      return 'aboutChannelDev';
    case BrowserChannel.STABLE:
      return 'aboutChannelStable';
    case BrowserChannel.LTC:
      return 'aboutChannelLongTermSupportCandidate';
    case BrowserChannel.LTS:
      return 'aboutChannelLongTermSupport';
  }
}

/**
 * Returns whether the target channel is more stable than the current channel.
 */
export function isTargetChannelMoreStable(
    currentChannel: BrowserChannel, targetChannel: BrowserChannel): boolean {
  // List of channels in increasing stability order.
  const channelList = [
    BrowserChannel.CANARY,
    BrowserChannel.DEV,
    BrowserChannel.BETA,
    BrowserChannel.STABLE,
    BrowserChannel.LTC,
    BrowserChannel.LTS,
  ];
  const currentIndex = channelList.indexOf(currentChannel);
  const targetIndex = channelList.indexOf(targetChannel);
  return currentIndex < targetIndex;
}

export interface AboutPageBrowserProxy {
  /**
   * Applies deferred update if it exists.
   */
  applyDeferredUpdate(): void;

  /**
   * Indicates to the browser that the page is ready.
   */
  pageReady(): void;

  /**
   * Request update status from the browser. It results in one or more
   * 'update-status-changed' WebUI events.
   */
  refreshUpdateStatus(): void;

  /** Opens the release notes app. */
  launchReleaseNotes(): void;

  // <if expr="_google_chrome">
  /**
   * Opens the feedback dialog.
   */
  openFeedbackDialog(): void;
  // </if>

  /** Opens the diagnostics page. */
  openDiagnostics(): void;

  /** Opens the "other open source software" license page. */
  openProductLicenseOther(): void;

  /** Opens the OS help page. */
  openOsHelpPage(): void;

  /** Opens the firmware updates page. */
  openFirmwareUpdatesPage(): void;

  /**
   * Requests the number of firmware updates.
   */
  getFirmwareUpdateCount(): Promise<number>;

  /**
   * Checks for available update and applies if it exists.
   */
  requestUpdate(): void;

  /**
   * Checks for the update with specified version and size and applies over
   * cellular. The target version and size are the same as were received from
   * 'update-status-changed' WebUI event. We send this back all the way to
   * update engine for it to double check with update server in case there's a
   * new update available. This prevents downloading the new update that user
   * didn't agree to.
   */
  requestUpdateOverCellular(targetVersion: string, targetSize: string): void;

  setChannel(channel: BrowserChannel, isPowerwashAllowed: boolean): void;

  /**
   * Requests channel info from the version updater. This may have latency if
   * the version updater is busy, for example with downloading updates.
   */
  getChannelInfo(): Promise<ChannelInfo>;

  canChangeChannel(): Promise<boolean>;

  getVersionInfo(): Promise<VersionInfo>;

  getRegulatoryInfo(): Promise<RegulatoryInfo|null>;

  /**
   * Checks if the device has reached end-of-life status and will no longer
   * receive updates.
   */
  getEndOfLifeInfo(): Promise<EndOfLifeInfo>;

  /**
   * Called when the end of life incentive button is clicked.
   */
  endOfLifeIncentiveButtonClicked(): void;

  /**
   * Request TPM firmware update status from the browser. It results in one or
   * more 'tpm-firmware-update-status-changed' WebUI events.
   */
  refreshTpmFirmwareUpdateStatus(): void;

  /**
   * Checks if the device is connected to the internet.
   */
  checkInternetConnection(): Promise<boolean>;

  isManagedAutoUpdateEnabled(): Promise<boolean>;

  isConsumerAutoUpdateEnabled(): Promise<boolean>;

  setConsumerAutoUpdate(enable: boolean): void;

  /**
   * Checks if the device is currently eligible for opt in.
   * @param eolPassed Whether end of life date has passed.
   * @param extendedDatePassed Whether extended updates date has passed.
   * @param extendedOptInRequired Whether opt-in is required for the device.
   */
  isExtendedUpdatesOptInEligible(
      eolPassed: boolean, extendedDatePassed: boolean,
      extendedOptInRequired: boolean): Promise<boolean>;

  /**
   * Opens the extended updates opt-in dialog.
   */
  openExtendedUpdatesDialog(): void;

  /**
   * Records that the Extended Updates option was shown to the user.
   */
  recordExtendedUpdatesShown(): void;
}

let instance: AboutPageBrowserProxy|null = null;

export class AboutPageBrowserProxyImpl implements AboutPageBrowserProxy {
  static getInstance(): AboutPageBrowserProxy {
    return instance || (instance = new AboutPageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: AboutPageBrowserProxy): void {
    instance = obj;
  }

  applyDeferredUpdate(): void {
    chrome.send('applyDeferredUpdate');
  }

  pageReady(): void {
    chrome.send('aboutPageReady');
  }

  refreshUpdateStatus(): void {
    chrome.send('refreshUpdateStatus');
  }

  launchReleaseNotes(): void {
    chrome.send('launchReleaseNotes');
  }

  // <if expr="_google_chrome">
  openFeedbackDialog(): void {
    chrome.send('openFeedbackDialog');
  }
  // </if>

  openDiagnostics(): void {
    chrome.send('openDiagnostics');
  }

  openProductLicenseOther(): void {
    chrome.send('openProductLicenseOther');
  }

  openOsHelpPage(): void {
    chrome.send('openOsHelpPage');
  }

  openFirmwareUpdatesPage(): void {
    chrome.send('openFirmwareUpdatesPage');
  }

  getFirmwareUpdateCount(): Promise<number> {
    return sendWithPromise('getFirmwareUpdateCount');
  }

  requestUpdate(): void {
    chrome.send('requestUpdate');
  }

  requestUpdateOverCellular(targetVersion: string, targetSize: string): void {
    chrome.send('requestUpdateOverCellular', [targetVersion, targetSize]);
  }

  setChannel(channel: BrowserChannel, isPowerwashAllowed: boolean): void {
    chrome.send('setChannel', [channel, isPowerwashAllowed]);
  }

  getChannelInfo(): Promise<ChannelInfo> {
    return sendWithPromise('getChannelInfo');
  }

  canChangeChannel(): Promise<boolean> {
    return sendWithPromise('canChangeChannel');
  }

  getVersionInfo(): Promise<VersionInfo> {
    return sendWithPromise('getVersionInfo');
  }

  getRegulatoryInfo(): Promise<RegulatoryInfo|null> {
    return sendWithPromise('getRegulatoryInfo');
  }

  getEndOfLifeInfo(): Promise<EndOfLifeInfo> {
    return sendWithPromise('getEndOfLifeInfo');
  }

  endOfLifeIncentiveButtonClicked(): void {
    chrome.send('openEndOfLifeIncentive');
  }

  checkInternetConnection(): Promise<boolean> {
    return sendWithPromise('checkInternetConnection');
  }

  refreshTpmFirmwareUpdateStatus(): void {
    chrome.send('refreshTPMFirmwareUpdateStatus');
  }

  isManagedAutoUpdateEnabled(): Promise<boolean> {
    return sendWithPromise('isManagedAutoUpdateEnabled');
  }

  isConsumerAutoUpdateEnabled(): Promise<boolean> {
    return sendWithPromise('isConsumerAutoUpdateEnabled');
  }

  setConsumerAutoUpdate(enable: boolean): void {
    chrome.send('setConsumerAutoUpdate', [enable]);
  }

  isExtendedUpdatesOptInEligible(
      eolPassed: boolean, extendedDatePassed: boolean,
      extendedOptInRequired: boolean): Promise<boolean> {
    return sendWithPromise(
        'isExtendedUpdatesOptInEligible', eolPassed, extendedDatePassed,
        extendedOptInRequired);
  }

  openExtendedUpdatesDialog(): void {
    chrome.send('openExtendedUpdatesDialog');
  }

  recordExtendedUpdatesShown(): void {
    chrome.send('recordExtendedUpdatesShown');
  }
}
