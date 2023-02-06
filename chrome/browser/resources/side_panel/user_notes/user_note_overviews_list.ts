// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './user_note_overview_row.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './user_note_overviews_list.html.js';
import {NoteOverview} from './user_notes.mojom-webui.js';
import {UserNotesApiProxy, UserNotesApiProxyImpl} from './user_notes_api_proxy.js';

export class UserNoteOverviewsListElement extends PolymerElement {
  static get is() {
    return 'user-note-overviews-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      overviews: Object,
    };
  }

  overviews: NoteOverview[];

  private userNotesApi_: UserNotesApiProxy =
      UserNotesApiProxyImpl.getInstance();

  private sortByModificationTime_(
      overview1: NoteOverview, overview2: NoteOverview): number {
    return Number(
        overview2.lastModificationTime.internalValue -
        overview1.lastModificationTime.internalValue);
  }

  private onRowClicked_(
      event: CustomEvent<{overview: NoteOverview, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    if (event.detail.overview.isCurrentTab) {
      this.dispatchEvent(new CustomEvent('current-tab-overview-selected', {
        bubbles: true,
        composed: true,
      }));
    } else {
      this.userNotesApi_.noteOverviewSelected(event.detail.overview.url, {
        middleButton: false,
        altKey: event.detail.event.altKey,
        ctrlKey: event.detail.event.ctrlKey,
        metaKey: event.detail.event.metaKey,
        shiftKey: event.detail.event.shiftKey,
      });
    }
  }

  private onShowContextMenuClicked_(
      event: CustomEvent<{overview: NoteOverview, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    // TODO(crbug.com/1409894): Implement this.
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'user-note-overviews-list': UserNoteOverviewsListElement;
  }
}

customElements.define(
    UserNoteOverviewsListElement.is, UserNoteOverviewsListElement);