// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './user_note_menu.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './user_note.html.js';
import {Note} from './user_notes.mojom-webui.js';
import {UserNotesApiProxy, UserNotesApiProxyImpl} from './user_notes_api_proxy.js';

export interface UserNoteElement {
  $: {
    noteContent: HTMLElement,
  };
}

export class UserNoteElement extends PolymerElement {
  static get is() {
    return 'user-note';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The `note` is undefined if the UserNoteElement is the persistent entry
       * note and not a preexisting note.
       */
      note: {
        type: Object,
        observer: 'onNoteChanged_',
      },

      editing_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      characterCounter_: {
        type: Number,
        computed: 'computeCharacterCounter_(noteContent_)',
      },

      showPlaceholder_: {
        type: Boolean,
        computed: 'computeShowPlaceholder_(noteContent_)',
      },

      noteContent_: {
        type: String,
        value: '',
      },
    };
  }

  note?: Note;
  private characterCounter_: string;
  private editing_: boolean;
  private noteContent_: string;
  private showPlaceholder_: boolean;

  private userNotesApi_: UserNotesApiProxy =
      UserNotesApiProxyImpl.getInstance();

  override ready() {
    super.ready();
    this.editing_ = this.note === undefined;
  }

  private onNoteContentInput_() {
    this.noteContent_ = this.$.noteContent.textContent!;
  }

  private computeCharacterCounter_(): number {
    return this.noteContent_.length;
  }

  private computeShowPlaceholder_(): boolean {
    return this.noteContent_.length === 0;
  }

  private onNoteChanged_() {
    if (this.note) {
      this.$.noteContent.textContent = this.note.text;
    }
    this.onNoteContentInput_();
  }

  private getContentEditable_() {
    return this.editing_ ? 'plaintext-only' : 'false';
  }

  private clearInput_() {
    this.$.noteContent.textContent = '';
    this.onNoteContentInput_();
  }

  private onCancelClick_() {
    if (this.note === undefined) {
      this.clearInput_();
    } else {
      this.$.noteContent.textContent = this.note.text;
      this.onNoteContentInput_();
      this.editing_ = false;
    }
  }

  private async onAddClick_() {
    if (this.note === undefined) {
      await this.userNotesApi_.newNoteFinished(this.$.noteContent.textContent!);
      this.clearInput_();
    } else {
      await this.userNotesApi_.updateNote(
          this.note.guid, this.$.noteContent.textContent!);
      this.editing_ = false;
    }
  }

  private onEditClicked_() {
    this.editing_ = true;
    setTimeout(() => {
      this.$.noteContent.focus();
    }, 0);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-note': UserNoteElement;
  }
}

customElements.define(UserNoteElement.is, UserNoteElement);
