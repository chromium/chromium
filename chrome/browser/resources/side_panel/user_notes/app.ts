// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../strings.m.js';
import './user_note.js';

import {listenOnce} from 'chrome://resources/js/util_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {Note} from './user_notes.mojom-webui.js';
import {UserNotesApiProxy, UserNotesApiProxyImpl} from './user_notes_api_proxy.js';

export interface UserNotesAppElement {
  $: {
    notesList: HTMLElement,
  };
}

export class UserNotesAppElement extends PolymerElement {
  static get is() {
    return 'user-notes-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      notes_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private notes_: Note[];
  private userNotesApi_: UserNotesApiProxy =
      UserNotesApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.userNotesApi_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.notesChanged.addListener(() => {
          this.updateNotes();
        }),
        callbackRouter.currentTabUrlChanged.addListener(() => {
          this.updateNotes();
        }),
    );

    listenOnce(this.$.notesList, 'dom-change', () => {
      // Push the ShowUi() callback to the event queue to allow deferred
      // rendering to take place.
      this.userNotesApi_.showUi();
    });
    this.updateNotes();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.userNotesApi_.getCallbackRouter().removeListener(id));
    this.listenerIds_ = [];
  }

  /**
   * Fetches the latest notes from the browser.
   */
  private async updateNotes() {
    const {notes} = await this.userNotesApi_.getNotesForCurrentTab();
    this.notes_ = notes;
  }

  private sortByModificationTime_(note1: Note, note2: Note): number {
    return Number(
        note1.lastModificationTime.internalValue -
        note2.lastModificationTime.internalValue);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-notes-app': UserNotesAppElement;
  }
}

customElements.define(UserNotesAppElement.is, UserNotesAppElement);
