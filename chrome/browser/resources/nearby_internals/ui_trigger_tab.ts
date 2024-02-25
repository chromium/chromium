// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './ui_trigger_list_object.js';
import './cross_device_internals.js';
import './shared_style.css.js';

import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyPrefsBrowserProxy} from './nearby_prefs_browser_proxy.js';
import {NearbyUiTriggerBrowserProxy} from './nearby_ui_trigger_browser_proxy.js';
import type {NearbyShareStates, ShareTarget, ShareTargetSelectOption, StatusCode, TimestampedMessage, TransferMetadataStatus} from './types.js';
import {ShareTargetDiscoveryChange} from './types.js';
import {getTemplate} from './ui_trigger_tab.html.js';


const UiTriggerTabElementBase = WebUiListenerMixin(PolymerElement);

class UiTriggerTabElement extends UiTriggerTabElementBase {
  static get is() {
    return 'ui-trigger-tab';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {

      uiTriggerObjectList_: {
        type: Array,
        value: () => [],
      },

      shareTargetSelectOptionList_: {
        type: Array,
        value: () => [],
      },

      /**ID of the selected ShareTarget or ''*/
      selectedShareTargetId_: String,

    };
  }

  private uiTriggerObjectList_?: TimestampedMessage[];
  private shareTargetSelectOptionList_?: ShareTargetSelectOption[];
  private selectedShareTargetId_: string;
  private browserProxy_: NearbyUiTriggerBrowserProxy =
      NearbyUiTriggerBrowserProxy.getInstance();
  private prefsBrowserProxy_: NearbyPrefsBrowserProxy =
      NearbyPrefsBrowserProxy.getInstance();

  /**
   * When the page is initialized, notify the C++ layer to allow JavaScript and
   * initialize WebUI Listeners.
   */
  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'transfer-updated',
        (transferUpdate: TransferMetadataStatus) =>
            this.onTransferUpdateAdded_(transferUpdate));
    this.addWebUiListener(
        'share-target-discovered',
        (shareTarget: ShareTarget) =>
            this.onShareTargetDiscovered_(shareTarget));
    this.addWebUiListener(
        'share-target-lost',
        (shareTarget: ShareTarget) => this.onShareTargetLost_(shareTarget));
    this.addWebUiListener(
        'on-status-code-returned',
        (statusCode: StatusCode) => this.onStatusCodeReturned_(statusCode));
    this.addWebUiListener(
        'share-target-map-updated',
        (shareTargetMapUpdate: ShareTarget[]) =>
            this.onShareTargetMapChanged_(shareTargetMapUpdate));
    this.browserProxy_.initialize();
  }

  /**
   * Triggers RegisterSendSurface with Foreground as Send state.
   */
  private onRegisterSendSurfaceForegroundClicked_(): void {
    this.browserProxy_.registerSendSurfaceForeground().then(
        (statusCode: StatusCode) => this.onStatusCodeReturned_(statusCode));
  }

  /**
   * Triggers RegisterSendSurface with Background as Send state.
   */
  private onRegisterSendSurfaceBackgroundClicked_(): void {
    this.browserProxy_.registerSendSurfaceBackground().then(
        (statusCode: StatusCode) => this.onStatusCodeReturned_(statusCode));
  }

  /**
   * Triggers UnregisterSendSurface.
   */
  private onUnregisterSendSurfaceClicked_(): void {
    this.browserProxy_.unregisterSendSurface().then(
        (statusCode: StatusCode) => this.onStatusCodeReturned_(statusCode));
  }

  /**
   * Clears Nearby Share Prefs.
   */
  private onClearPrefsButtonClicked_(): void {
    this.prefsBrowserProxy_.clearNearbyPrefs();
  }

  private onFastPairErrorNotificationClicked_(): void {
    this.browserProxy_.notifyFastPairError();
  }

  private onFastPairDiscoveryNotificationClicked_(): void {
    this.browserProxy_.notifyFastPairDiscovery();
  }

  private onFastPairPairingNotificationClicked_(): void {
    this.browserProxy_.notifyFastPairPairing();
  }

  private onFastPairDeviceApplicationAvailableNotificationClicked_(): void {
    this.browserProxy_.notifyFastPairApplicationAvailable();
  }

  private onFastPairDeviceApplicationInstalledNotificationClicked_(): void {
    this.browserProxy_.notifyFastPairApplicationInstalled();
  }

  private onFastPairAssociateAccountNotificationClicked_(): void {
    this.browserProxy_.notifyFastPairAssociateAccount();
  }

  /**
   * Triggers RegisterReceiveSurface with Foreground as Receive state.
   */
  private onRegisterReceiveSurfaceForegroundClicked_(): void {
    this.browserProxy_.registerReceiveSurfaceForeground().then(
        (statusCode: StatusCode) => this.onStatusCodeReturned_(statusCode));
  }

  /**
   * Triggers RegisterReceiveSurface with Background as Receive state.
   */
  private onRegisterReceiveSurfaceBackgroundClicked_(): void {
    this.browserProxy_.registerReceiveSurfaceBackground().then(
        (statusCode: StatusCode) => this.onStatusCodeReturned_(statusCode));
  }

  /**
   * Triggers UnregisterReceiveSurface.
   */
  private onUnregisterReceiveSurfaceClicked_(): void {
    this.browserProxy_.unregisterReceiveSurface().then(
        (statusCode: StatusCode) => this.onStatusCodeReturned_(statusCode));
  }

  /**
   * Logs status code returned by triggered events.
   */
  private onStatusCodeReturned_(statusCode: StatusCode): void {
    const message =
        statusCode.triggerEvent + ' Result: ' + statusCode.statusCode;
    const time = statusCode.time;
    this.unshift('uiTriggerObjectList_', {'message': message, 'time': time});
  }

  /**
   * Updates state variables based on the dictionary returned once triggered
   * by |GetState|.
   */
  private onCurrentStatesReturned_(currentStates: NearbyShareStates): void {
    const time = currentStates.time;
    const message =
        `Is Scanning? : ${currentStates.isScanning}\nIs Transferring? : ${
            currentStates.isTransferring}\nIs Receiving? : ${
            currentStates.isReceiving}\nIs Sending? : ${
            currentStates.isSending}\nIs Connecting? : ${
            currentStates.isConnecting}\nIs In High Visibility? : ${
            currentStates.isInHighVisibility}`;
    this.unshift('uiTriggerObjectList_', {'message': message, 'time': time});
  }

  /**
   * Triggers sendText with selected |shareTargetId|.
   */
  private onSendTextClicked_(): void {
    this.browserProxy_.sendText(this.selectedShareTargetId_)
        .then(
            (statusCode: StatusCode) => this.onStatusCodeReturned_(statusCode));
  }

  /**
   * Triggers Accept with selected |shareTargetId|.
   */
  private onAcceptClicked_(): void {
    this.browserProxy_.accept(this.selectedShareTargetId_);
  }

  /**
   * Triggers Reject with selected |shareTargetId|.
   */
  private onRejectClicked_(): void {
    this.browserProxy_.reject(this.selectedShareTargetId_);
  }

  /**
   * Triggers Cancel with selected |shareTargetId|.
   */
  private onCancelClicked_(): void {
    this.browserProxy_.cancel(this.selectedShareTargetId_);
  }

  /**
   * Triggers Open with selected |shareTargetId|.
   */
  private onOpenClicked_(): void {
    this.browserProxy_.open(this.selectedShareTargetId_);
  }


  /**
   * Triggers GetState to retrieve current states and update display
   * accordingly.
   */
  private onGetStatesClicked_(): void {
    this.browserProxy_.getState().then(
        (currentStates: NearbyShareStates) =>
            this.onCurrentStatesReturned_(currentStates));
  }

  /**
   * Triggers ShowNearbyShareReceivedNotification which displays a Nearby Share
   * "Received" notification.
   */
  private onNearbyShareReceivedNotificationClicked_(): void {
    this.browserProxy_.showNearbyShareReceivedNotification();
  }

  /**
   * Updates |selectedShareTargetId_| with the new selected option.
   */
  private onSelectChange_(): void {
    const elem: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#share-select');
    if (elem) {
      this.selectedShareTargetId_ = elem.value;
    }
  }

  /**
   * Parses an array of ShareTargets and adds to the JavaScript list
   * |shareTargetSelectOptionList_| to be displayed in select list.
   */
  private onShareTargetMapChanged_(shareTargetMapUpdate: ShareTarget[]): void {
    this.shareTargetSelectOptionList_ = [];
    shareTargetMapUpdate.forEach((shareTarget) => {
      const name = `${shareTarget.deviceName} (${shareTarget.shareTargetId})`;
      const value = shareTarget.shareTargetId;
      const selected = value === this.selectedShareTargetId_;
      this.push(
          'shareTargetSelectOptionList_',
          {'name': name, 'selected': selected, 'value': value});
    });
  }

  /**
   * Handles ShareTargets when they are discovered in the C++.
   */
  private onShareTargetDiscovered_(shareTarget: ShareTarget): void {
    this.convertShareTargetToTimestampedMessageAndAppendToList_(
        shareTarget, ShareTargetDiscoveryChange.DISCOVERED);
  }

  /**
   * Handles ShareTargets when they are lost in the C++.
   */
  private onShareTargetLost_(shareTarget: ShareTarget): void {
    this.convertShareTargetToTimestampedMessageAndAppendToList_(
        shareTarget, ShareTargetDiscoveryChange.LOST);
  }

  /**
   * Adds |transferUpdate| sent in from WebUI listener to the displayed list.
   */
  private onTransferUpdateAdded_(transferUpdate: TransferMetadataStatus): void {
    this.convertTransferUpdateTimestampedMessageAndAppendToList_(
        transferUpdate);
  }

  /**
   * Converts |transferUpdate| sent in to a generic object to be displayed.
   */
  private convertTransferUpdateTimestampedMessageAndAppendToList_(
      transferUpdate: TransferMetadataStatus): void {
    const time = transferUpdate.time;
    const message =
        `${transferUpdate.deviceName} (${transferUpdate.shareTargetId}): ${
            transferUpdate.transferMetadataStatus}`;
    this.unshift('uiTriggerObjectList_', {'message': message, 'time': time});
  }

  /**
   * Converts |statusCode| sent in to a generic object to be displayed.
   */
  private convertStatusCodeToTimestampedMessageAndAppendToList_(
      statusCode: StatusCode): void {
    const time = statusCode.time;
    const message = `${statusCode.triggerEvent} ${statusCode.statusCode}`;
    this.unshift('uiTriggerObjectList_', {'message': message, 'time': time});
  }

  /**
   * Converts |shareTarget| sent in to when discovered/lost a generic object to
   * be displayed.
   */
  private convertShareTargetToTimestampedMessageAndAppendToList_(
      shareTarget: ShareTarget,
      discoveryChange: ShareTargetDiscoveryChange): void {
    const time = shareTarget.time;
    const message = `${shareTarget.deviceName} (${shareTarget.shareTargetId}) ${
        this.shareTargetDirectionToString_(discoveryChange)}`;
    this.unshift('uiTriggerObjectList_', {'message': message, 'time': time});
  }

  /**
   * Sets the string representation of ShareTargetDiscoveryChange
   * |discoveryChange|.
   */
  private shareTargetDirectionToString_(
      discoveryChange: ShareTargetDiscoveryChange): (string|undefined) {
    switch (discoveryChange) {
      case ShareTargetDiscoveryChange.DISCOVERED:
        return 'discovered';
      case ShareTargetDiscoveryChange.LOST:
        return 'lost';
      default:
        return;
    }
  }
}

customElements.define(UiTriggerTabElement.is, UiTriggerTabElement);
