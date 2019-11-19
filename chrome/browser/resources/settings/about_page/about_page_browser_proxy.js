// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "About" section to interact with
 * the browser.
 */

// <if expr="chromeos">
/**
 * @typedef {{
 *   text: string,
 *   url: string,
 * }}
 */
let RegulatoryInfo;

/**
 * @typedef {{
 *   currentChannel: BrowserChannel,
 *   targetChannel: BrowserChannel,
 *   canChangeChannel: boolean,
 * }}
 */
let ChannelInfo;

/**
 * @typedef {{
 *   version: (string|undefined),
 *   size: (string|undefined),
 * }}
 */
let AboutPageUpdateInfo;

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
const BrowserChannel = {
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
let TPMFirmwareUpdateStatusChangedEvent;
// </if>

/**
 * Enumeration of all possible update statuses. The string literals must match
 * the ones defined at |AboutHandler::UpdateStatusToString|.
 * @enum {string}
 */
const UpdateStatus = {
  CHECKING: 'checking',
  UPDATING: 'updating',
  NEARLY_UPDATED: 'nearly_updated',
  UPDATED: 'updated',
  FAILED: 'failed',
  DISABLED: 'disabled',
  DISABLED_BY_ADMIN: 'disabled_by_admin',
  NEED_PERMISSION_TO_UPDATE: 'need_permission_to_update',
};

// <if expr="_google_chrome and is_macosx">
/**
 * @typedef {{
 *   hidden: boolean,
 *   disabled: boolean,
 *   actionable: boolean,
 *   text: (string|undefined)
 * }}
 */
let PromoteUpdaterStatus;
// </if>

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
let UpdateStatusChangedEvent;

cr.define('settings', function() {
  /**
   * @param {!BrowserChannel} channel
   * @return {string}
   */
  function browserChannelToI18nId(channel) {
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
  function isTargetChannelMoreStable(currentChannel, targetChannel) {
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
  class AboutPageBrowserProxy {
    /**
     * Indicates to the browser that the page is ready.
     */
    pageReady() {}

    /**
     * Request update status from the browser. It results in one or more
     * 'update-status-changed' WebUI events.
     */
    refreshUpdateStatus() {}

    // <if expr="chromeos">
    /** Opens the release notes app. */
    launchReleaseNotes() {}
    // </if>


    /** Opens the help page. */
    openHelpPage() {}

    // <if expr="_google_chrome">
    /**
     * Opens the feedback dialog.
     */
    openFeedbackDialog() {}

    // </if>

    // <if expr="chromeos">
    /** Opens the OS help page. */
    openOsHelpPage() {}

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

    /** @return {!Promise<!ChannelInfo>} */
    getChannelInfo() {}

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
    // </if>

    // <if expr="_google_chrome and is_macosx">
    /**
     * Triggers setting up auto-updates for all users.
     */
    promoteUpdater() {}
    // </if>

    // <if expr="chromeos">
    /**
     * Checks if the device has release notes enabled.
     * @return {!Promise<boolean>}
     */
    getEnabledReleaseNotes() {}

    /**
     * Checks if the device is connected to the internet.
     * @return {!Promise<boolean>}
     */
    checkInternetConnection() {}
    // </if>
  }

  /**
   * @implements {settings.AboutPageBrowserProxy}
   */
  class AboutPageBrowserProxyImpl {
    /** @override */
    pageReady() {
      chrome.send('aboutPageReady');
    }

    /** @override */
    refreshUpdateStatus() {
      chrome.send('refreshUpdateStatus');
    }

    // <if expr="_google_chrome and is_macosx">
    /** @override */
    promoteUpdater() {
      chrome.send('promoteUpdater');
    }

    // </if>

    /** @override */
    launchReleaseNotes() {
      chrome.send('launchReleaseNotes');
    }

    /** @override */
    openHelpPage() {
      chrome.send('openHelpPage');
    }

    // <if expr="_google_chrome">
    /** @override */
    openFeedbackDialog() {
      chrome.send('openFeedbackDialog');
    }

    // </if>

    // <if expr="chromeos">
    /** @override */
    openOsHelpPage() {
      chrome.send('openOsHelpPage');
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
      return cr.sendWithPromise('getChannelInfo');
    }

    /** @override */
    getRegulatoryInfo() {
      return cr.sendWithPromise('getRegulatoryInfo');
    }

    /** @override */
    getEndOfLifeInfo() {
      return cr.sendWithPromise('getEndOfLifeInfo');
    }

    /** @override */
    getEnabledReleaseNotes() {
      return cr.sendWithPromise('getEnabledReleaseNotes');
    }

    /** @override */
    checkInternetConnection() {
      return cr.sendWithPromise('checkInternetConnection');
    }

    /** @override */
    refreshTPMFirmwareUpdateStatus() {
      chrome.send('refreshTPMFirmwareUpdateStatus');
    }
    // </if>
  }

  cr.addSingletonGetter(AboutPageBrowserProxyImpl);

  return {
    AboutPageBrowserProxy: AboutPageBrowserProxy,
    AboutPageBrowserProxyImpl: AboutPageBrowserProxyImpl,
    browserChannelToI18nId: browserChannelToI18nId,
    isTargetChannelMoreStable: isTargetChannelMoreStable,
  };
});
