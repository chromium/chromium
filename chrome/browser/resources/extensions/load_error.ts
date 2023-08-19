// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './code_section.js';
import './strings.m.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionsCodeSectionElement} from './code_section.js';
import {getTemplate} from './load_error.html.js';

export interface LoadErrorDelegate {
  /**
   * Attempts to load the previously-attempted unpacked extension.
   */
  retryLoadUnpacked(retryGuid?: string): Promise<boolean>;
}

export interface ExtensionsLoadErrorElement {
  $: {
    code: ExtensionsCodeSectionElement,
    dialog: CrDialogElement,
  };
}

export class ExtensionsLoadErrorElement extends PolymerElement {
  static get is() {
    return 'extensions-load-error';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,
      loadError: Object,
      file_: {
        type: String,
        value: null,
      },
      error_: {
        type: String,
        value: null,
      },
      retrying_: Boolean,
    };
  }

  static get observers() {
    return [
      'observeLoadErrorChanges_(loadError)',
    ];
  }

  delegate: LoadErrorDelegate;
  loadError: Error|chrome.developerPrivate.LoadError;

  private file_?: string;
  private error_?: string;
  private retrying_: boolean;

  show() {
    this.$.dialog.showModal();
  }

  close() {
    this.$.dialog.close();
  }

  private onRetryClick_() {
    this.retrying_ = true;
    this.delegate
        .retryLoadUnpacked(
            this.loadError instanceof Error ? undefined :
                                              this.loadError.retryGuid)
        .then(
            () => {
              this.close();
            },
            loadError => {
              this.loadError = loadError;
              this.retrying_ = false;
            });
  }

  private observeLoadErrorChanges_() {
    assert(this.loadError);

    if (this.loadError instanceof Error) {
      this.file_ = undefined;
      this.error_ = this.loadError.message;
      this.$.code.isActive = false;
      return;
    }

    this.file_ = this.loadError.path;
    this.error_ = this.loadError.error;

    const source = this.loadError.source;
    // CodeSection expects a RequestFileSourceResponse, rather than an
    // ErrorFileSource. Massage into place.
    // TODO(devlin): Make RequestFileSourceResponse use ErrorFileSource.
    const codeSectionProperties = {
      beforeHighlight: source ? source.beforeHighlight : '',
      highlight: source ? source.highlight : '',
      afterHighlight: source ? source.afterHighlight : '',
      title: '',
      message: this.loadError.error,
    };

    this.$.code.code = codeSectionProperties;
    this.$.code.isActive = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-load-error': ExtensionsLoadErrorElement;
  }
}

customElements.define(
    ExtensionsLoadErrorElement.is, ExtensionsLoadErrorElement);
