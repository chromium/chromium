// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'edit-hostname-dialog' is a component allowing the
 * user to edit the device hostname.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeviceNameBrowserProxy, DeviceNameBrowserProxyImpl} from './device_name_browser_proxy.js';
import {SetDeviceNameResult} from './device_name_util.js';
import {getTemplate} from './edit_hostname_dialog.html.js';

const MAX_INPUT_LENGTH = 15;

const MIN_INPUT_LENGTH = 1;

const UNALLOWED_CHARACTERS = '[^0-9A-Za-z-]+';

const EMOJI_REGEX_EXP =
    /(\u00a9|\u00ae|[\u2000-\u3300]|\ud83c[\ud000-\udfff]|\ud83d[\ud000-\udfff]|\ud83e[\ud000-\udfff])/gi;

export interface EditHostnameDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const EditHostnameDialogElementBase = I18nMixin(PolymerElement);

export class EditHostnameDialogElement extends EditHostnameDialogElementBase {
  static get is() {
    return 'edit-hostname-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      deviceName_: {
        type: String,
        value: '',
        observer: 'onDeviceNameChanged_',
      },

      isInputInvalid_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      inputCountString_: {
        type: String,
        computed: 'computeInputCountString_(deviceName_)',
      },
    };
  }

  private deviceName_: string;
  private isInputInvalid_: boolean;
  private inputCountString_: string;

  private deviceNameBrowserProxy_: DeviceNameBrowserProxy;

  constructor() {
    super();

    this.deviceNameBrowserProxy_ = DeviceNameBrowserProxyImpl.getInstance();
  }

  /**
   * Returns a formatted string containing the current number of characters
   * entered in the input compared to the maximum number of characters allowed.
   */
  private computeInputCountString_(): string {
    return this.i18n(
        'aboutDeviceNameInputCharacterCount',
        this.deviceName_.length.toLocaleString(),
        MAX_INPUT_LENGTH.toLocaleString());
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  /**
   * Observer for deviceName_ that sanitizes its value by removing any
   * Emojis and truncating it to MAX_INPUT_LENGTH. This method will be
   * recursively called until deviceName_ is fully sanitized.
   */
  private onDeviceNameChanged_(_newValue: string, oldValue: string): void {
    if (oldValue) {
      const sanitizedOldValue = oldValue.replace(EMOJI_REGEX_EXP, '');
      // If sanitizedOldValue.length > MAX_INPUT_LENGTH, the user attempted to
      // enter more than the max limit, this method was called and it was
      // truncated, and then this method was called one more time.
      this.isInputInvalid_ = sanitizedOldValue.length > MAX_INPUT_LENGTH;
    } else {
      this.isInputInvalid_ = false;
    }

    // Remove all Emojis from the name.
    const sanitizedDeviceName = this.deviceName_.replace(EMOJI_REGEX_EXP, '');

    // Truncate the name to MAX_INPUT_LENGTH.
    this.deviceName_ = sanitizedDeviceName.substring(0, MAX_INPUT_LENGTH);

    if (this.deviceName_.length < MIN_INPUT_LENGTH ||
        this.deviceName_.match(UNALLOWED_CHARACTERS)) {
      this.isInputInvalid_ = true;
    }
  }

  private onDoneClick_(): void {
    this.deviceNameBrowserProxy_.attemptSetDeviceName(this.deviceName_)
        .then(result => {
          this.handleSetDeviceNameResponse_(result);
        });
    this.$.dialog.close();
  }

  private handleSetDeviceNameResponse_(result: SetDeviceNameResult): void {
    if (result !== SetDeviceNameResult.UPDATE_SUCCESSFUL) {
      console.error('ERROR IN UPDATE', result);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EditHostnameDialogElement.is]: EditHostnameDialogElement;
  }
}

customElements.define(EditHostnameDialogElement.is, EditHostnameDialogElement);
