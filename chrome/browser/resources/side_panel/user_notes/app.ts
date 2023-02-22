// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://user-notes-side-panel.top-chrome/shared/sp_heading.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../strings.m.js';
import './user_note_overviews_list.js';
import '../user_notes_list.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {Note, NoteOverview} from './user_notes.mojom-webui.js';
import {UserNotesApiProxy, UserNotesApiProxyImpl} from './user_notes_api_proxy.js';

export interface UserNotesAppElement {
  $: {
    pageNotesList: HTMLElement,
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

      overviews_: {
        type: Array,
        value: () => [],
      },

      viewingOverviews_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      startNoteCreation_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private notes_: Array<(Note | null)>;
  private overviews_: NoteOverview[];
  private viewingOverviews_: boolean;
  private startNoteCreation_: boolean;
  private userNotesApi_: UserNotesApiProxy =
      UserNotesApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.userNotesApi_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.notesChanged.addListener(() => {
          if (this.viewingOverviews_) {
            this.updateNoteOverviews_();
          } else {
            this.updateNotes_();
          }
        }),
        callbackRouter.currentTabUrlChanged.addListener(
            async (startNoteCreation: boolean) => {
              this.viewingOverviews_ = false;
              await this.updateNotes_();
              this.startNoteCreation_ = startNoteCreation;
            }),
        callbackRouter.startNoteCreation.addListener(async () => {
          if (this.viewingOverviews_) {
            await this.updateNotes_();
            this.viewingOverviews_ = false;
          }
          this.startNoteCreation_ = true;
        }),
    );
    if (this.viewingOverviews_) {
      this.updateNoteOverviews_();
    } else {
      this.updateNotes_();
    }
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
  private async updateNotes_() {
    const {notes} = await this.userNotesApi_.getNotesForCurrentTab();
    this.notes_ = notes;
    // Add a null note which becomes the UserNoteElement that is the persistent
    // entry point for creating a new note.
    this.notes_.push(null);
  }

  /**
   * Fetches the latest note overviews from the browser.
   */
  private async updateNoteOverviews_() {
    const {overviews} = await this.userNotesApi_.getNoteOverviews('');
    this.overviews_ = overviews;
  }

  private onCurrentTabOverviewSelected_() {
    this.viewingOverviews_ = false;
  }

  private onAllNotesClick_() {
    this.updateNoteOverviews_();
    this.viewingOverviews_ = true;
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
