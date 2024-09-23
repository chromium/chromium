// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-reset-dialog' is a dialog for
 * triggering factory resets of security keys.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared.css.js';
import '../i18n_setup.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SecurityKeysResetBrowserProxy} from './security_keys_browser_proxy.js';
import {SecurityKeysResetBrowserProxyImpl} from './security_keys_browser_proxy.js';
import {getTemplate} from './security_keys_reset_dialog.html.js';

export enum ResetDialogPage {
  INITIAL = 'initial',
  NO_RESET = 'noReset',
  RESET_FAILED = 'resetFailed',
  RESET_CONFIRM = 'resetConfirm',
  RESET_SUCCESS = 'resetSuccess',
  RESET_NOT_ALLOWED = 'resetNotAllowed',
}

export interface SettingsSecurityKeysResetDialogElement {
  $: {
    button: HTMLElement,
    dialog: CrDialogElement,
    resetFailed: HTMLElement,
  };
}

const SettingsSecurityKeysResetDialogElementBase = I18nMixin(PolymerElement);

export class SettingsSecurityKeysResetDialogElement extends
    SettingsSecurityKeysResetDialogElementBase {
  static get is() {
    return 'settings-security-keys-reset-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * A CTAP error code for when the specific error was not recognised.
       */
      errorCode_: Number,

      /**
       * True iff the process has completed, successfully or otherwise.
       */
      complete_: {
        type: Boolean,
        value: false,
      },

      /**
       * The id of an element on the page that is currently shown.
       */
      shown_: {
        type: String,
        value: ResetDialogPage.INITIAL,
      },

      title_: String,
    };
  }

  private errorCode_: number;
  private complete_: boolean;
  private shown_: ResetDialogPage;
  private title_: string;
  private browserProxy_: SecurityKeysResetBrowserProxy =
      SecurityKeysResetBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.title_ = this.i18n('securityKeysResetTitle');
    this.$.dialog.showModal();

    this.browserProxy_.reset().then(code => {
      // code is a CTAP error code. See
      // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#error-responses
      if (code === 1 /* INVALID_COMMAND */) {
        this.shown_ = ResetDialogPage.NO_RESET;
        this.finish_();
      } else if (code !== 0 /* unknown error */) {
        this.errorCode_ = code;
        this.shown_ = ResetDialogPage.RESET_FAILED;
        this.finish_();
      } else {
        this.title_ = this.i18n('securityKeysResetConfirmTitle');
        this.shown_ = ResetDialogPage.RESET_CONFIRM;
        this.browserProxy_.completeReset().then(code => {
          this.title_ = this.i18n('securityKeysResetTitle');
          if (code === 0 /* SUCCESS */) {
            this.shown_ = ResetDialogPage.RESET_SUCCESS;
          } else if (code === 48 /* NOT_ALLOWED */) {
            this.shown_ = ResetDialogPage.RESET_NOT_ALLOWED;
          } else /* unknown error */ {
            this.errorCode_ = code;
            this.shown_ = ResetDialogPage.RESET_FAILED;
          }
          this.finish_();
        });
      }
    });
  }

  private closeDialog_() {
    this.$.dialog.close();
    this.finish_();
  }

  private finish_() {
    if (this.complete_) {
      return;
    }
    this.complete_ = true;
    this.browserProxy_.close();
  }

  private onIronSelect_(e: Event) {
    // Prevent this event from bubbling since it is unnecessarily triggering
    // the listener within settings-animated-pages.
    e.stopPropagation();
  }

  /**
   * @param code CTAP error code.
   * @return Contents of the error string that may be displayed to the user.
   *     Used automatically by Polymer.
   */
  private resetFailed_(code: number): string {
    if (code === null) {
      return '';
    }
    return this.i18n('securityKeysResetError', code.toString());
  }

  /**
   * @param complete Whether the dialog process is complete.
   * @return The label of the dialog button. Used automatically by Polymer.
   */
  private closeText_(complete: boolean): string {
    return this.i18n(complete ? 'ok' : 'cancel');
  }

  /**
   * @param complete Whether the dialog process is complete.
   * @return The class of the dialog button. Used automatically by Polymer.
   */
  private maybeActionButton_(complete: boolean): string {
    return complete ? 'action-button' : 'cancel-button';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-keys-reset-dialog':
        SettingsSecurityKeysResetDialogElement;
  }
}

customElements.define(
    SettingsSecurityKeysResetDialogElement.is,
    SettingsSecurityKeysResetDialogElement);
