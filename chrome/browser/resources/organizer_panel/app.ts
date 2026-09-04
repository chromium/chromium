// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import './organizer_list.js';
import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {OpenTabsDelegate} from './delegates/open_tabs_delegate.js';
import {RecentTabsDelegate} from './delegates/recent_tabs_delegate.js';
import {TabGroupsDelegate} from './delegates/tab_groups_delegate.js';
import type {OrganizerListElement} from './organizer_list.js';
import type {OrganizerListSectionDelegate} from './organizer_list_section_delegate.js';

export interface OrganizerPanelAppElement {
  $: {
    list: OrganizerListElement,
    searchField: CrToolbarSearchFieldElement,
  };
}

export class OrganizerPanelAppElement extends CrLitElement {
  static get is() {
    return 'organizer-panel-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shortcut_: {type: String},
      sectionDelegates_: {type: Array},
      searchQuery_: {type: String},
    };
  }

  protected accessor shortcut_: string = loadTimeData.getString('shortcutText');
  protected accessor searchQuery_: string = '';
  protected accessor sectionDelegates_:
      Array<OrganizerListSectionDelegate<unknown>> = [
        new OpenTabsDelegate(),
        new RecentTabsDelegate(),
        new TabGroupsDelegate(),
      ];


  protected onSearchChanged_(e: CustomEvent<string>) {
    this.searchQuery_ = e.detail;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-panel-app': OrganizerPanelAppElement;
  }
}

customElements.define(OrganizerPanelAppElement.is, OrganizerPanelAppElement);
