// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {RememberedNote} from '../ai_overlay_dialog.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';

export interface NotesAppElement {
  $: {
    newNoteKey: CrInputElement,
    newNoteValue: CrInputElement,
    addNoteButton: CrButtonElement,
  };
}

export class NotesAppElement extends CrLitElement {
  static get is() {
    return 'ai-overlay-notes-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      notes: {type: Array},
    };
  }

  protected accessor notes: RememberedNote[] = [];

  private browserProxy_: BrowserProxy = BrowserProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.fetchNotes_();
  }

  private async fetchNotes_() {
    const {notes} = await this.browserProxy_.handler.getRememberedNotes();
    this.notes = notes || [];
  }

  protected async onNoteValueChange_(e: Event) {
    const input = e.target as CrInputElement;
    const key = input.dataset['key'];
    assert(key);

    const value = input.value.trim();
    if (!value) {
      return;
    }

    await this.browserProxy_.handler.setRememberedNote({key, value});
    this.fetchNotes_();
  }

  protected async onDeleteNoteClick_(e: Event) {
    const button = e.target as HTMLElement;
    const key = button.dataset['key'];
    assert(key);

    await this.browserProxy_.handler.setRememberedNote({key, value: ''});
    this.fetchNotes_();
  }

  protected async onAddNoteClick_() {
    const key = this.$.newNoteKey.value.trim();
    const value = this.$.newNoteValue.value.trim();
    if (!key || !value) {
      return;
    }

    await this.browserProxy_.handler.setRememberedNote({key, value});
    this.$.newNoteKey.value = '';
    this.$.newNoteValue.value = '';
    this.fetchNotes_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-overlay-notes-app': NotesAppElement;
  }
}

customElements.define(NotesAppElement.is, NotesAppElement);
