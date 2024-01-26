// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input_style.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getTemplate} from './credential_note.html.js';

export interface CredentialNoteElement {
  $: {
    noteValue: HTMLElement,
    showMore: HTMLAnchorElement,
  };
}

const CredentialNoteElementBase = I18nMixin(PolymerElement);

export class CredentialNoteElement extends CredentialNoteElementBase {
  static get is() {
    return 'credential-note';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      note: String,
    };
  }

  note: string;
  private showNoteFully_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    // Set default value here so listeners can be updated properly.
    this.showNoteFully_ = false;
  }

  private getNoteValue_(): string {
    return !this.note ? this.i18n('emptyNote') : this.note!;
  }

  private noteIsEmpty_(): boolean {
    return !this.note;
  }

  private isNoteFullyVisible_(): boolean {
    return this.showNoteFully_ ||
        this.$.noteValue.scrollHeight === this.$.noteValue.offsetHeight;
  }

  private onshowMoreClick_(e: Event) {
    e.preventDefault();
    this.showNoteFully_ = true;
    PasswordManagerImpl.getInstance().extendAuthValidity();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'credential-note': CredentialNoteElement;
  }
}

customElements.define(CredentialNoteElement.is, CredentialNoteElement);
