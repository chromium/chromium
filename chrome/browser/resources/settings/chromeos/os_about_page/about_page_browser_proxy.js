// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "About" section to interact with
 * the browser.
 */

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @typedef {{
 *   text: string,
 *   url: string,
 * }}
 */
export let RegulatoryInfo;

/**
 * @typedef {{
 *   currentChannel: BrowserChannel,
 *   targetChannel: BrowserChannel,
 *   isLts: boolean,
 * }}
 */
export let ChannelInfo;

/**
 * @typedef {{
 *   arcVersion: string,
 *   osFirmware: string,
 *   osVersion: string,
 * }}
 */
export let VersionInfo;

/**
 * @typedef {{
 *   version: (string|undefined),
 *   size: (string|undefined),
 * }}
 */
export let AboutPageUpdateInfo;

/**
 * @typedef {{
 *   hasEndOfLife: (boolean|undefined),
 *   eolMessageWithMonthAndYear: (string|undefined),
 * }}
 */
let EndOfLifeInfo;

/**
 * Enumeration of all possible browser channels.
 * @enum {string}
 */
export const BrowserChannel = {
  BETA: 'beta-channel',
  CANARY: 'canary-channel',
  DEV: 'dev-channel',
  STABLE: 'stable-channel',
};

/**
 * @typedef {{
 *   updateAvailable: boolean,
 * }}
 */
export let TPMFirmwareUpdateStatusChangedEvent;

/**
 * Enumeration of all possible update statuses. The string literals must match
 * the ones defined at |AboutHandler::UpdateStatusToString|.
 * @enum {string}
 */
export const UpdateStatus = {
  CHECKING: 'checking',
  UPDATING: 'updating',
  NEARLY_UPDATED: 'nearly_updated',
  UPDATED: 'updated',
  FAILED: 'failed',
  FAILED_HTTP: 'failed_http',
  FAILED_DOWNLOAD: 'failed_download',
  DISABLED: 'disabled',
  DISABLED_BY_ADMIN: 'disabled_by_admin',
  NEED_PERMISSION_TO_UPDATE: 'need_permission_to_update',
};

/**
 * @typedef {{
 *   status: !UpdateStatus,
 *   progress: (number|undefined),
 *   message: (string|undefined),
 *   connectionTypes: (string|undefined),
 *   version: (string|undefined),
 *   size: (string|undefined),
 * }}
 */
export let UpdateStatusChangedEvent;

/**
 * @param {!BrowserChannel} channel
 * @param {boolean} isLts
 * @return {string}
 */
export function browserChannelToI18nId(channel, isLts) {
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
  }

  assertNotReached();
}

/**
 * @param {!BrowserChannel} currentChannel
 * @param {!BrowserChannel} targetChannel
 * @return {boolean} Whether the target channel is more stable than the
 *     current channel.
 */
export function isTargetChannelMoreStable(currentChannel, targetChannel) {
  // List of channels in increasing stability order.
  const channelList = [
    BrowserChannel.CANARY,
    BrowserChannel.DEV,
    BrowserChannel.BETA,
    BrowserChannel.STABLE,
  ];
  const currentIndex = channelList.indexOf(currentChannel);
  const targetIndex = channelList.indexOf(targetChannel);
  return currentIndex < targetIndex;
}

/** @interface */
export class AboutPageBrowserProxy {
  /**
   * Indicates to the browser that the page is ready.
   */
  pageReady() {}

  /**
   * Request update status from the browser. It results in one or more
   * 'update-status-changed' WebUI events.
   */
  refreshUpdateStatus() {}

  /** Opens the release notes app. */
  launchReleaseNotes() {}

  // <if expr="_google_chrome">
  /**
   * Opens the feedback dialog.
   */
  openFeedbackDialog() {}

  // </if>

  /** Opens the diagnostics page. */
  openDiagnostics() {}

  /** Opens the OS help page. */
  openOsHelpPage() {}

  /** Opens the firmware updates page. */
  openFirmwareUpdatesPage() {}

  /**
   * Checks for available update and applies if it exists.
   */
  requestUpdate() {}

  /**
   * Checks for the update with specified version and size and applies over
   * cellular. The target version and size are the same as were received from
   * 'update-status-changed' WebUI event. We send this back all the way to
   * update engine for it to double check with update server in case there's a
   * new update available. This prevents downloading the new update that user
   * didn't agree to.
   * @param {string} target_version
   * @param {string} target_size
   */
  requestUpdateOverCellular(target_version, target_size) {}

  /**
   * @param {!BrowserChannel} channel
   * @param {boolean} isPowerwashAllowed
   */
  setChannel(channel, isPowerwashAllowed) {}

  /**
   * Requests channel info from the version updater. This may have latency if
   * the version updater is busy, for example with downloading updates.
   * @return {!Promise<!ChannelInfo>}
   */
  getChannelInfo() {}

  /** @return {!Promise<!boolean>} */
  canChangeChannel() {}

  /** @return {!Promise<!VersionInfo>} */
  getVersionInfo() {}

  /** @return {!Promise<?RegulatoryInfo>} */
  getRegulatoryInfo() {}

  /**
   * Checks if the device has reached end-of-life status and will no longer
   * receive updates.
   * @return {!Promise<!EndOfLifeInfo>}
   */
  getEndOfLifeInfo() {}

  /**
   * Request TPM firmware update status from the browser. It results in one or
   * more 'tpm-firmware-update-status-changed' WebUI events.
   */
  refreshTPMFirmwareUpdateStatus() {}

  /**
   * Checks if the device is connected to the internet.
   * @return {!Promise<boolean>}
   */
  checkInternetConnection() {}

  /** @return {!Promise<boolean>} */
  isManagedAutoUpdateEnabled() {}

  /** @return {!Promise<boolean>} */
  isConsumerAutoUpdateEnabled() {}

  /**
   * @param {boolean} enable
   */
  setConsumerAutoUpdate(enable) {}
}

/** @type {?AboutPageBrowserProxy} */
let instance = null;

/**
 * @implements {AboutPageBrowserProxy}
 */
export class AboutPageBrowserProxyImpl {
  /** @return {!AboutPageBrowserProxy} */
  static getInstance() {
    return instance || (instance = new AboutPageBrowserProxyImpl());
  }

  /** @param {!AboutPageBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }

  /** @override */
  pageReady() {
    chrome.send('aboutPageReady');
  }

  /** @override */
  refreshUpdateStatus() {
    chrome.send('refreshUpdateStatus');
  }

  /** @override */
  launchReleaseNotes() {
    chrome.send('launchReleaseNotes');
  }

  // <if expr="_google_chrome">
  /** @override */
  openFeedbackDialog() {
    chrome.send('openFeedbackDialog');
  }

  // </if>

  /** @override */
  openDiagnostics() {
    chrome.send('openDiagnostics');
  }

  /** @override */
  openOsHelpPage() {
    chrome.send('openOsHelpPage');
  }

  /** @override */
  openFirmwareUpdatesPage() {
    chrome.send('openFirmwareUpdatesPage');
  }

  /** @override */
  requestUpdate() {
    chrome.send('requestUpdate');
  }

  /** @override */
  requestUpdateOverCellular(target_version, target_size) {
    chrome.send('requestUpdateOverCellular', [target_version, target_size]);
  }

  /** @override */
  setChannel(channel, isPowerwashAllowed) {
    chrome.send('setChannel', [channel, isPowerwashAllowed]);
  }

  /** @override */
  getChannelInfo() {
    return sendWithPromise('getChannelInfo');
  }

  /** @override */
  canChangeChannel() {
    return sendWithPromise('canChangeChannel');
  }

  /** @override */
  getVersionInfo() {
    return sendWithPromise('getVersionInfo');
  }

  /** @override */
  getRegulatoryInfo() {
    return sendWithPromise('getRegulatoryInfo');
  }

  /** @override */
  getEndOfLifeInfo() {
    return sendWithPromise('getEndOfLifeInfo');
  }

  /** @override */
  checkInternetConnection() {
    return sendWithPromise('checkInternetConnection');
  }

  /** @override */
  refreshTPMFirmwareUpdateStatus() {
    chrome.send('refreshTPMFirmwareUpdateStatus');
  }

  /** @override */
  isManagedAutoUpdateEnabled() {
    return sendWithPromise('isManagedAutoUpdateEnabled');
  }

  /** @override */
  isConsumerAutoUpdateEnabled() {
    return sendWithPromise('isConsumerAutoUpdateEnabled');
  }

  /** @override */
  setConsumerAutoUpdate(enable) {
    chrome.send('setConsumerAutoUpdate', [enable]);
  }
}
