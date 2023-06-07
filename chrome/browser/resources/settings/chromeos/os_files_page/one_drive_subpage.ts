// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OneDriveBrowserProxy} from './one_drive_browser_proxy.js';
import {getTemplate} from './one_drive_subpage.html.js';

const SettingsOneDriveSubpageElementBase = I18nMixin(PolymerElement);

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
      connected_: {
        type: Boolean,
        value() {
          return false;
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

  private connected_: boolean;
  private userEmailAddress_: string|null;
  private oneDriveProxy_: OneDriveBrowserProxy;

  override connectedCallback() {
    super.connectedCallback();
    this.oneDriveProxy_.observer.onODFSMountOrUnmount.addListener(
        this.updateUserEmailAddress_.bind(this));
  }

  private async updateUserEmailAddress_() {
    const {email} = await this.oneDriveProxy_.handler.getUserEmailAddress();
    this.userEmailAddress_ = email;
    this.connected_ = email !== null;
  }

  private signedInAsLabel_(connected: boolean) {
    if (connected) {
      assert(this.userEmailAddress_);
      return this.i18nAdvanced(
          'oneDriveSignedInAs',
          {tags: ['strong'], substitutions: [this.userEmailAddress_]});
    }
    return this.i18n('oneDriveDisconnected');
  }

  private connectDisconnectButtonLabel_(connected: boolean) {
    return this.i18n(connected ? 'oneDriveDisconnect' : 'oneDriveConnect');
  }

  private async onConnectDisconnectButtonClick_(): Promise<void> {
    if (this.connected_) {
      const {success}: {success: boolean} =
          await this.oneDriveProxy_.handler.disconnectFromOneDrive();
      if (!success) {
        console.error('Disconnecting from OneDrive failed');
      }
    } else {
      const {success}: {success: boolean} =
          await this.oneDriveProxy_.handler.connectToOneDrive();
      if (!success) {
        console.error('Connecting to OneDrive failed');
      }
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
