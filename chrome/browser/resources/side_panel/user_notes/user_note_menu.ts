// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './user_note_menu.html.js';
import {Note} from './user_notes.mojom-webui.js';
import {UserNotesApiProxy, UserNotesApiProxyImpl} from './user_notes_api_proxy.js';

export interface UserNoteMenuElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export class UserNoteMenuElement extends PolymerElement {
  static get is() {
    return 'user-note-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      note: Object,
    };
  }

  note: Note;
  private userNotesApi_: UserNotesApiProxy =
      UserNotesApiProxyImpl.getInstance();

  private onMenuButtonClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.menu.showAt(event.target as HTMLElement);
  }

  private onEditClick_() {
    this.dispatchEvent(new CustomEvent('edit-clicked', {
      bubbles: true,
      composed: true,
    }));
    this.$.menu.close();
  }

  private onDeleteClick_() {
    this.userNotesApi_.deleteNote(this.note.guid);
    this.$.menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-note-menu': UserNoteMenuElement;
  }
}

customElements.define(UserNoteMenuElement.is, UserNoteMenuElement);
