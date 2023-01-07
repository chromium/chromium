// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './pack_dialog_alert.js';
import './strings.m.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './pack_dialog.html.js';

export interface PackDialogDelegate {
  /**
   * Opens a file browser for the user to select the root directory.
   * @return A promise that is resolved with the path the user selected.
   */
  choosePackRootDirectory(): Promise<string>;

  /**
   * Opens a file browser for the user to select the private key file.
   * @return A promise that is resolved with the path the user selected.
   */
  choosePrivateKeyPath(): Promise<string>;

  /** Packs the extension into a .crx. */
  packExtension(
      rootPath: string, keyPath: string, flag?: number,
      callback?:
          (response: chrome.developerPrivate.PackDirectoryResponse) => void):
      void;
}

export interface ExtensionsPackDialogElement {
  $: {
    dialog: CrDialogElement,
    keyFileBrowse: HTMLElement,
    keyFile: CrInputElement,
    rootDirBrowse: HTMLElement,
    rootDir: CrInputElement,
  };
}

export class ExtensionsPackDialogElement extends PolymerElement {
  static get is() {
    return 'extensions-pack-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,

      packDirectory_: {
        type: String,
        value: '',  // Initialized to trigger binding when attached.
      },

      keyFile_: String,
      lastResponse_: Object,
    };
  }

  delegate: PackDialogDelegate;
  private packDirectory_: string;
  private keyFile_: string;
  private lastResponse_: chrome.developerPrivate.PackDirectoryResponse|null;

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private onRootBrowse_() {
    this.delegate.choosePackRootDirectory().then(path => {
      if (path) {
        this.set('packDirectory_', path);
      }
    });
  }

  private onKeyBrowse_() {
    this.delegate.choosePrivateKeyPath().then(path => {
      if (path) {
        this.set('keyFile_', path);
      }
    });
  }

  private onCancelTap_() {
    this.$.dialog.cancel();
  }

  private onConfirmTap_() {
    this.delegate.packExtension(
        this.packDirectory_, this.keyFile_, 0, this.onPackResponse_.bind(this));
  }

  /**
   * @param response The response from request to pack an extension.
   */
  private onPackResponse_(response:
                              chrome.developerPrivate.PackDirectoryResponse) {
    this.lastResponse_ = response;
  }

  /**
   * In the case that the alert dialog was a success message, the entire
   * pack-dialog should close. Otherwise, we detach the alert by setting
   * lastResponse_ null. Additionally, if the user selected "proceed anyway"
   * in the dialog, we pack the extension again with override flags.
   */
  private onAlertClose_(e: Event) {
    e.stopPropagation();

    if (this.lastResponse_!.status ===
        chrome.developerPrivate.PackStatus.SUCCESS) {
      this.$.dialog.close();
      return;
    }

    // This is only possible for a warning dialog.
    if (this.shadowRoot!.querySelector(
                            'extensions-pack-dialog-alert')!.returnValue ===
        'success') {
      this.delegate.packExtension(
          this.lastResponse_!.item_path, this.lastResponse_!.pem_path,
          this.lastResponse_!.override_flags, this.onPackResponse_.bind(this));
    }

    this.lastResponse_ = null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-pack-dialog': ExtensionsPackDialogElement;
  }
}

customElements.define(
    ExtensionsPackDialogElement.is, ExtensionsPackDialogElement);
