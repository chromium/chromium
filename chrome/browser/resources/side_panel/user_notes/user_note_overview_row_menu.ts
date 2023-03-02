// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './user_note_overview_row_menu.html.js';
import {NoteOverview} from './user_notes.mojom-webui.js';
import {UserNotesApiProxy, UserNotesApiProxyImpl} from './user_notes_api_proxy.js';

export interface UserNoteOverviewRowMenuElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export class UserNoteOverviewRowMenuElement extends PolymerElement {
  static get is() {
    return 'user-note-overview-row-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      overview: Object,
    };
  }

  overview: NoteOverview;
  private userNotesApi_: UserNotesApiProxy =
      UserNotesApiProxyImpl.getInstance();

  private onMenuButtonClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.menu.showAt(event.target as HTMLElement);
  }

  private onAddNoteClick_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    if (this.overview.isCurrentTab) {
      this.dispatchEvent(new CustomEvent('current-tab-overview-selected', {
        bubbles: true,
        composed: true,
      }));
    } else {
      this.userNotesApi_.noteOverviewSelected(this.overview.url, {
        middleButton: false,
        altKey: false,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      });
    }
    this.dispatchEvent(new CustomEvent('start-note-creation', {
      bubbles: true,
      composed: true,
    }));
    this.$.menu.close();
  }

  private onOpenInNewTabClick_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.userNotesApi_.openInNewTab(this.overview.url);
    this.$.menu.close();
  }

  private onOpenInNewWindowClick_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.userNotesApi_.openInNewWindow(this.overview.url);
    this.$.menu.close();
  }

  private onOpenInIncognitoWindowClick_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.userNotesApi_.openInIncognitoWindow(this.overview.url);
    this.$.menu.close();
  }

  private onDeleteClick_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.userNotesApi_.deleteNotesForUrl(this.overview.url);
    this.$.menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-note-overview-row-menu': UserNoteOverviewRowMenuElement;
  }
}

customElements.define(
    UserNoteOverviewRowMenuElement.is, UserNoteOverviewRowMenuElement);