// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './code_section.js';
import './strings.m.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CodeSectionElement} from './code_section.js';
import {getCss} from './load_error.css.js';
import {getHtml} from './load_error.html.js';

export interface LoadErrorDelegate {
  /**
   * Attempts to load the previously-attempted unpacked extension.
   */
  retryLoadUnpacked(retryGuid?: string): Promise<boolean>;
}

export interface LoadErrorElement {
  $: {
    code: CodeSectionElement,
    dialog: CrDialogElement,
  };
}

export class LoadErrorElement extends CrLitElement {
  static get is() {
    return 'extensions-load-error';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},
      loadError: {type: Object},
      file_: {type: String},
      error_: {type: String},
      retrying_: {type: Boolean},
      isCodeSectionActive_: {type: Boolean},
      codeSectionProperties_: {type: Object},
    };
  }

  static get observers() {
    return [
      'observeLoadErrorChanges_(loadError)',
    ];
  }

  delegate?: LoadErrorDelegate;
  loadError?: Error|chrome.developerPrivate.LoadError;

  protected codeSectionProperties_:
      chrome.developerPrivate.RequestFileSourceResponse|null = null;
  protected file_?: string;
  protected error_: string|null = null;
  protected isCodeSectionActive_?: boolean;
  protected retrying_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('loadError')) {
      assert(this.loadError);

      if (this.loadError instanceof Error) {
        this.file_ = undefined;
        this.error_ = this.loadError.message;
        this.isCodeSectionActive_ = false;
        return;
      }

      this.file_ = this.loadError.path;
      this.error_ = this.loadError.error;

      const source = this.loadError.source;
      // CodeSection expects a RequestFileSourceResponse, rather than an
      // ErrorFileSource. Massage into place.
      // TODO(devlin): Make RequestFileSourceResponse use ErrorFileSource.
      this.codeSectionProperties_ = {
        beforeHighlight: source ? source.beforeHighlight : '',
        highlight: source ? source.highlight : '',
        afterHighlight: source ? source.afterHighlight : '',
        title: '',
        message: this.loadError.error,
      };
      this.isCodeSectionActive_ = true;
    }
  }

  show() {
    this.$.dialog.showModal();
  }

  close() {
    this.$.dialog.close();
  }

  protected onRetryClick_() {
    this.retrying_ = true;
    assert(this.delegate);
    assert(this.loadError);
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
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-load-error': LoadErrorElement;
  }
}

customElements.define(LoadErrorElement.is, LoadErrorElement);
