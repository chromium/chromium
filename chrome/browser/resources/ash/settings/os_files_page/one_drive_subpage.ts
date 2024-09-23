// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExhaustive} from '../assert_extras.js';

import {OneDriveBrowserProxy} from './one_drive_browser_proxy.js';
import {getTemplate} from './one_drive_subpage.html.js';

const SettingsOneDriveSubpageElementBase = I18nMixin(PolymerElement);

export const enum OneDriveConnectionState {
  LOADING = 'loading',
  CONNECTED = 'connected',
  DISCONNECTED = 'disconnected',
}

export class SettingsOneDriveSubpageElement extends
    SettingsOneDriveSubpageElementBase {
  static get is() {
    return 'settings-one-drive-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
         @private Indicates whether the user is connected to OneDrive or not.
        */
      connectionState_: {
        type: String,
        value() {
          return OneDriveConnectionState.LOADING;
        },
      },
    };
  }

  /**
   * Resolved once the async calls initiated by the constructor have resolved.
   */
  initPromise: Promise<void>;

  constructor() {
    super();
    this.oneDriveProxy_ = OneDriveBrowserProxy.getInstance();
    this.initPromise = this.updateUserEmailAddress_();
  }

  private connectionState_: OneDriveConnectionState;
  private userEmailAddress_: string|null;
  private oneDriveProxy_: OneDriveBrowserProxy;
  private allowUserToRemoveOdfs_: boolean = true;

  override connectedCallback(): void {
    super.connectedCallback();
    this.oneDriveProxy_.observer.onODFSMountOrUnmount.addListener(
        this.updateUserEmailAddress_.bind(this));
    this.oneDriveProxy_.observer.onAllowUserToRemoveODFSChanged.addListener(
        this.updateAllowUserToRemoveOdfs_.bind(this));
  }

  updateConnectionStateForTesting(connectionState: OneDriveConnectionState):
      void {
    this.connectionState_ = connectionState;
  }

  private async updateUserEmailAddress_(): Promise<void> {
    this.connectionState_ = OneDriveConnectionState.LOADING;
    const {email} = await this.oneDriveProxy_.handler.getUserEmailAddress();
    this.userEmailAddress_ = email;
    this.connectionState_ = email === null ?
        OneDriveConnectionState.DISCONNECTED :
        OneDriveConnectionState.CONNECTED;
  }

  private updateAllowUserToRemoveOdfs_(isAllowed: boolean): void {
    this.allowUserToRemoveOdfs_ = isAllowed;
  }

  private isConnected_(): boolean {
    return this.connectionState_ === OneDriveConnectionState.CONNECTED;
  }

  private isLoading_(): boolean {
    return this.connectionState_ === OneDriveConnectionState.LOADING;
  }

  private isRemoveAccessDisabled_(): boolean {
    return this.isLoading_() || !this.allowUserToRemoveOdfs_;
  }

  private signedInAsLabel_(): TrustedHTML {
    switch (this.connectionState_) {
      case OneDriveConnectionState.CONNECTED:
        assert(this.userEmailAddress_);
        return this.i18nAdvanced(
            'oneDriveSignedInAs',
            {tags: ['strong'], substitutions: [this.userEmailAddress_]});
      case OneDriveConnectionState.DISCONNECTED:
        return this.i18nAdvanced('oneDriveDisconnected');
      case OneDriveConnectionState.LOADING:
        return this.i18nAdvanced('oneDriveLoading');
      default:
        assertExhaustive(this.connectionState_);
    }
  }

  private connectDisconnectButtonLabel_(): string {
    return this.i18n(
        this.connectionState_ === OneDriveConnectionState.CONNECTED ?
            'oneDriveDisconnect' :
            'oneDriveConnect');
  }

  private async onConnectDisconnectButtonClick_(): Promise<void> {
    switch (this.connectionState_) {
      case OneDriveConnectionState.CONNECTED: {
        const {success}: {success: boolean} =
            await this.oneDriveProxy_.handler.disconnectFromOneDrive();
        if (!success) {
          console.error('Disconnecting from OneDrive failed');
        }
        break;
      }
      case OneDriveConnectionState.DISCONNECTED: {
        const {success}: {success: boolean} =
            await this.oneDriveProxy_.handler.connectToOneDrive();
        if (!success) {
          console.error('Connecting to OneDrive failed');
        }
        break;
      }
      case OneDriveConnectionState.LOADING:
        console.warn('Connect button clicked when connection state is loading');
        break;
      default:
        assertExhaustive(this.connectionState_);
    }
    // The UI is updated by listening to `onODFSMountOrUnmount`.
  }

  private async onOpenOneDriveFolderClick_(): Promise<void> {
    const {success}: {success: boolean} =
        await this.oneDriveProxy_.handler.openOneDriveFolder();
    if (!success) {
      console.error('Opening OneDrive folder failed');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsOneDriveSubpageElement.is]: SettingsOneDriveSubpageElement;
  }
}

customElements.define(
    SettingsOneDriveSubpageElement.is, SettingsOneDriveSubpageElement);
