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
import '../../i18n_setup.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SecurityKeysResetBrowserProxy} from './security_keys_browser_proxy.js';
import {SecurityKeysResetBrowserProxyImpl} from './security_keys_browser_proxy.js';
import {getCss} from './security_keys_reset_dialog.css.js';
import {getHtml} from './security_keys_reset_dialog.html.js';

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
    button: CrButtonElement,
    dialog: CrDialogElement,
    resetFailed: HTMLElement,
  };
}

const SettingsSecurityKeysResetDialogElementBase = I18nMixinLit(CrLitElement);

export class SettingsSecurityKeysResetDialogElement extends
    SettingsSecurityKeysResetDialogElementBase {
  static get is() {
    return 'settings-security-keys-reset-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * A CTAP error code for when the specific error was not recognised.
       */
      errorCode_: {type: Number},

      /**
       * True iff the process has completed, successfully or otherwise.
       */
      complete_: {type: Boolean},

      /**
       * The id of an element on the page that is currently shown.
       */
      shown_: {type: String},

      title_: {type: String},
    };
  }

  protected accessor errorCode_: number|null = null;
  protected accessor complete_: boolean = false;
  protected accessor shown_: ResetDialogPage = ResetDialogPage.INITIAL;
  protected accessor title_: string = '';
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

  protected onDialogClose_() {
    this.closeDialog_();
  }

  protected onButtonClick_() {
    this.closeDialog_();
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

  protected onIronSelect_(e: Event) {
    // Prevent this event from bubbling since it is unnecessarily triggering
    // the listener within settings-animated-pages.
    e.stopPropagation();
  }

  /**
   * @return Contents of the error string that may be displayed to the user.
   */
  protected resetFailed_(): string {
    if (this.errorCode_ === null) {
      return '';
    }
    return this.i18n('securityKeysResetError', this.errorCode_.toString());
  }

  /**
   * @return The label of the dialog button.
   */
  protected closeText_(): string {
    return this.i18n(this.complete_ ? 'ok' : 'cancel');
  }

  /**
   * @return The class of the dialog button.
   */
  protected maybeActionButton_(): string {
    return this.complete_ ? 'action-button' : 'cancel-button';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-keys-reset-dialog':
        SettingsSecurityKeysResetDialogElement;
  }
}

export type SecurityKeysResetDialogElement =
    SettingsSecurityKeysResetDialogElement;

customElements.define(
    SettingsSecurityKeysResetDialogElement.is,
    SettingsSecurityKeysResetDialogElement);
