// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getCss} from './credential_note.css.js';
import {getHtml} from './credential_note.html.js';

export interface CredentialNoteElement {
  $: {
    noteValue: HTMLElement,
    showMore: HTMLAnchorElement,
  };
}

const CredentialNoteElementBase = I18nMixinLit(CrLitElement);

export class CredentialNoteElement extends CredentialNoteElementBase {
  static get is() {
    return 'credential-note';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      note: {type: String},
      showNoteFully_: {type: Boolean},
      alwaysExpanded: {type: Boolean},
    };
  }

  accessor note: string = '';
  accessor alwaysExpanded: boolean = false;
  protected accessor showNoteFully_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    // Initialize visibility state based on whether the note should always be
    // expanded.
    if (changedProperties.has('alwaysExpanded')) {
      this.showNoteFully_ = this.alwaysExpanded;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedProperties.has('note') ||
        changedPrivateProperties.has('showNoteFully_')) {
      this.$.showMore.hidden = this.isNoteFullyVisible_();
    }
  }

  protected getNoteValue_(): string {
    return !this.note ? this.i18n('emptyNote') : this.note;
  }

  private isNoteFullyVisible_(): boolean {
    return this.showNoteFully_ ||
        this.$.noteValue.scrollHeight === this.$.noteValue.offsetHeight;
  }

  protected onShowMoreClick_(e: Event) {
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
