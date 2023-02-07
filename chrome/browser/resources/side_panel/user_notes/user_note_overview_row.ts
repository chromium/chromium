// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import '//user-notes-side-panel.top-chrome/shared/sp_list_item_badge.js';

import {CrUrlListItemSize} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './user_note_overview_row.html.js';
import {NoteOverview} from './user_notes.mojom-webui.js';

export class UserNoteOverviewRowElement extends PolymerElement {
  static get is() {
    return 'user-note-overview-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      overview: {
        type: Object,
        observer: 'onOverviewChanged_',
      },

      description: {
        type: String,
        value: '',
      },

      trailingIconAriaLabel: {
        type: String,
        value: '',
      },

      notesCount_: String,

      size_: {
        type: CrUrlListItemSize,
        value: CrUrlListItemSize.LARGE,
      },
    };
  }

  overview: NoteOverview;
  description: string;
  trailingIconAriaLabel: string;
  private notesCount_: string;
  private size_: CrUrlListItemSize;

  private getTitle_() {
    return this.overview.title === '' ? this.overview.url.url :
                                        this.overview.title;
  }

  private async onOverviewChanged_() {
    this.notesCount_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'notesCount', this.overview.numNotes);
  }

  private dispatchCustomEvent_(customEventType: string, event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent(customEventType, {
      bubbles: true,
      composed: true,
      detail: {
        overview: this.overview,
        event: event,
      },
    }));
  }

  /**
   * Dispatches a custom click event when the user clicks anywhere on the row.
   */
  private onRowClicked_(event: MouseEvent) {
    this.dispatchCustomEvent_('row-clicked', event);
  }

  /**
   * Dispatches a custom click event when the user right-clicks anywhere on the
   * row.
   */
  private onRowContextMenu_(event: MouseEvent) {
    this.dispatchCustomEvent_('context-menu', event);
  }

  /**
   * Dispatches a custom click event when the user clicks anywhere on the
   * trailing icon button.
   */
  private onTrailingIconClicked_(event: MouseEvent) {
    this.dispatchCustomEvent_('trailing-icon-clicked', event);
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'user-note-overview-row': UserNoteOverviewRowElement;
  }
}

customElements.define(
    UserNoteOverviewRowElement.is, UserNoteOverviewRowElement);