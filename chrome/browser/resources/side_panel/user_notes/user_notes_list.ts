// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://user-notes-side-panel.top-chrome/shared/sp_icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../strings.m.js';
import './user_note.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Note} from './user_notes.mojom-webui.js';
import {UserNotesApiProxy, UserNotesApiProxyImpl} from './user_notes_api_proxy.js';
import {getTemplate} from './user_notes_list.html.js';

export interface UserNotesListElement {
  $: {
    notesList: DomRepeat,
    sortMenu: CrActionMenuElement,
  };
}

export class UserNotesListElement extends PolymerElement {
  static get is() {
    return 'user-notes-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      notes: {
        type: Array,
        value: () => [],
      },

      startNoteCreation: {
        type: Boolean,
        notify: true,
        value: false,
      },

      activeSortIndex_: {
        type: Number,
        observer: 'onActiveSortIndexChanged_',
        value: function() {
          return loadTimeData.getBoolean('sortByNewest') ? 0 : 1;
        },
      },

      sortTypes_: {
        type: Array,
        value: () =>
            [loadTimeData.getString('sortNewest'),
             loadTimeData.getString('sortOldest')],
      },
    };
  }

  notes: Array<(Note | null)>;
  startNoteCreation: boolean;
  private activeSortIndex_: number;
  private sortTypes_: string[];
  private userNotesApi_: UserNotesApiProxy =
      UserNotesApiProxyImpl.getInstance();
  private listenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.userNotesApi_.getCallbackRouter();
    this.listenerId_ = callbackRouter.sortByNewestPrefChanged.addListener(
        (sortByNewest: boolean) => {
          const sortIndex = sortByNewest ? 0 : 1;
          if (this.activeSortIndex_ !== sortIndex) {
            this.activeSortIndex_ = sortIndex;
          }
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.listenerId_);
    this.userNotesApi_.getCallbackRouter().removeListener(this.listenerId_);
    this.listenerId_ = null;
  }

  private onAllNotesClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('all-notes-click', {
      bubbles: true,
      composed: true,
    }));
  }

  private onShowSortMenuClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.showAt(event.target as HTMLElement);
  }

  private getSortLabel_(): string {
    return this.sortTypes_[this.activeSortIndex_]!;
  }

  private getSortMenuItemLabel_(sortType: string): string {
    return loadTimeData.getStringF('sortByType', sortType);
  }

  private sortMenuItemIsSelected_(sortType: string): boolean {
    return this.sortTypes_[this.activeSortIndex_] === sortType;
  }

  private onSortTypeClicked_(event: DomRepeatEvent<string>) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    const sortByNewest = event.model.index === 0;
    this.userNotesApi_.setSortOrder(sortByNewest);
  }

  private onActiveSortIndexChanged_() {
    this.$.notesList.render();
  }

  private sortByModificationTime_(note1: Note|null, note2: Note|null): number {
    const sortByNewest = this.activeSortIndex_ === 0;
    if (note1 === null) {
      return sortByNewest ? -1 : 1;
    }
    if (note2 === null) {
      return sortByNewest ? 1 : -1;
    }
    const comp = Number(
        note1.lastModificationTime.internalValue -
        note2.lastModificationTime.internalValue);
    return sortByNewest ? -comp : comp;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-notes-list': UserNotesListElement;
  }
}

customElements.define(UserNotesListElement.is, UserNotesListElement);
