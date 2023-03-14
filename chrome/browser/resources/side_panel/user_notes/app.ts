// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://user-notes-side-panel.top-chrome/shared/sp_empty_state.js';
import 'chrome://user-notes-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://user-notes-side-panel.top-chrome/shared/sp_footer.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../strings.m.js';
import './user_note_overviews_list.js';
import '../user_notes_list.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {Note, NoteOverview} from './user_notes.mojom-webui.js';
import {UserNotesApiProxy, UserNotesApiProxyImpl} from './user_notes_api_proxy.js';

/** Enumeration of the possible states of the user notes side panel ui. */
export enum State {
  GUEST = 0,
  EMPTY = 1,
  OVERVIEWS = 2,
  CURRENT_PAGE_NOTES = 3,
}

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

      hasNotesInAnyPages_: {
        type: Boolean,
        value: false,
      },

      state_: {
        type: State,
        computed: 'computeState_(hasNotesInAnyPages_, viewingOverviews_)',
      },

      /**
       * This boolean is used indicate the user's preference in viewing the
       * overviews list or the list of notes for the current page.
       */
      viewingOverviews_: {
        type: Boolean,
        reflectToAttribute: true,
        value: true,
      },

      startNoteCreation_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private notes_: Array<(Note | null)>;
  private overviews_: NoteOverview[];
  private hasNotesInAnyPages_: boolean;
  private state_: State;
  private viewingOverviews_: boolean;
  private startNoteCreation_: boolean;
  private userNotesApi_: UserNotesApiProxy =
      UserNotesApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();

    if (this.isGuestMode_()) {
      return;
    }

    const callbackRouter = this.userNotesApi_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.notesChanged.addListener(() => {
          this.fetchAndUpdateNotesUi_(
              /*viewOverviews=*/ this.viewingOverviews_,
              /*startCreation=*/ false);
        }),
        callbackRouter.currentTabUrlChanged.addListener(
            async (startNoteCreation: boolean) => {
              await this.fetchAndUpdateNotesUi_(
                  /*viewOverviews=*/ false,
                  /*startCreation=*/ startNoteCreation);
              this.startNoteCreation_ = startNoteCreation;
            }),
        callbackRouter.startNoteCreation.addListener(async () => {
          if (this.viewingOverviews_) {
            await this.fetchAndUpdateNotesUi_(
                /*viewOverviews=*/ false, /*startCreation=*/ true);
          }
          this.startNoteCreation_ = true;
        }),
    );
    this.fetchAndUpdateNotesUi_(
        /*viewOverviews=*/ false, /*startCreation=*/ false);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.userNotesApi_.getCallbackRouter().removeListener(id));
    this.listenerIds_ = [];
  }

  private isGuestMode_() {
    return loadTimeData.getBoolean('guestMode');
  }

  private computeState_() {
    if (this.isGuestMode_()) {
      return State.GUEST;
    }
    if (!this.hasNotesInAnyPages_ && this.viewingOverviews_) {
      return State.EMPTY;
    }
    return this.viewingOverviews_ ? State.OVERVIEWS : State.CURRENT_PAGE_NOTES;
  }

  private async fetchAndUpdateNotesUi_(
      viewOverviews: boolean, startCreation: boolean) {
    // Check if there are user notes for any page. If not, clear notes_ and
    // overviews_ data.
    const {hasNotes} = await this.userNotesApi_.hasNotesInAnyPages();
    this.hasNotesInAnyPages_ = hasNotes;
    if (!hasNotes && !startCreation) {
      this.viewingOverviews_ = true;
      this.notes_ = [];
      this.overviews_ = [];
      return;
    }

    if (viewOverviews) {
      const {overviews} = await this.userNotesApi_.getNoteOverviews('');
      this.overviews_ = overviews;
      this.viewingOverviews_ = true;
    } else {
      const {notes} = await this.userNotesApi_.getNotesForCurrentTab();
      this.notes_ = notes;

      // If there are notes for the current tab or we should be starting the
      // creation flow then notes for the current page should be
      // shown. Otherwise, fall back to showing the overviews.
      if (notes.length > 0 || startCreation) {
        // Add a null note which becomes the UserNoteElement that is the
        // persistent entry point for creating a new note.
        this.notes_.push(null);
        this.viewingOverviews_ = false;
      } else {
        this.viewingOverviews_ = true;
        const {overviews} = await this.userNotesApi_.getNoteOverviews('');
        this.overviews_ = overviews;
      }
    }
  }

  private hasNotesForCurrentPage_() {
    // This also accounts for a null placeholder that is the persistent entry
    // point for creating a new note.
    return this.notes_.length > 1;
  }

  private shouldShowEmptyOrGuestState() {
    return this.state_ === State.EMPTY || this.state_ === State.GUEST;
  }

  private shouldShowAddButton_() {
    // The add button should be visible if we are in the empty state or if
    // viewing overviews and there are no notes for the current tab.
    return this.state_ === State.EMPTY ||
        (this.state_ === State.OVERVIEWS && !this.hasNotesForCurrentPage_());
  }

  private onAddNoteButtonClick_() {
    this.viewingOverviews_ = false;
    this.notes_ = [null];
    this.startNoteCreation_ = true;
  }

  private onCurrentTabOverviewSelected_() {
    this.viewingOverviews_ = false;
  }

  private onStartNoteCreation_() {
    this.startNoteCreation_ = true;
  }

  private onAllNotesClick_() {
    this.fetchAndUpdateNotesUi_(
        /*viewOverviews=*/ true, /*startCreation=*/ false);
  }

  private shouldShowOverviews_(): boolean {
    return this.state_ === State.OVERVIEWS;
  }

  private shouldShowPageNotes_(): boolean {
    return this.state_ === State.CURRENT_PAGE_NOTES;
  }

  private getEmptyTitle_(): string {
    if (this.isGuestMode_()) {
      return loadTimeData.getString('emptyTitleGuest');
    } else {
      return loadTimeData.getString('emptyTitle');
    }
  }

  private getEmptyBody_(): string {
    if (this.isGuestMode_()) {
      return loadTimeData.getString('emptyBodyGuest');
    } else {
      return loadTimeData.getString('emptyBody');
    }
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
