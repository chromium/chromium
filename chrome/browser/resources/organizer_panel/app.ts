// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import './organizer_list.js';
import '/strings.m.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {ForeignTabsDelegate} from './delegates/foreign_tabs_delegate.js';
import {OpenTabsDelegate} from './delegates/open_tabs_delegate.js';
import {RecentTabsDelegate} from './delegates/recent_tabs_delegate.js';
import {TabGroupsDelegate} from './delegates/tab_groups_delegate.js';
import type {OrganizerListElement} from './organizer_list.js';
import type {OrganizerListSectionDelegate} from './organizer_list_section_delegate.js';
import type {BrowserProxy} from './organizer_panel.mojom-webui.js';
import {browserProxyFactory} from './organizer_panel.mojom-webui.js';

export interface OrganizerPanelAppElement {
  $: {
    list: OrganizerListElement,
    menu: CrActionMenuElement,
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
      allSectionsExpanded_: {type: Boolean},
      shortcut_: {type: String},
      sectionDelegates_: {type: Array},
      searchQuery_: {type: String},
    };
  }

  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private onWindowFocused_: () => void = () => {
    // Autofocus only works when the window is initially shown; for subsequent
    // focus events, focus the search field.
    this.$.searchField.showAndFocus();
  };
  private onContextMenu_: (e: MouseEvent) => void = (e: MouseEvent) => {
    e.preventDefault();
    if (e.clientY > this.getBoundingClientRect().bottom) {
      this.browserProxy_.handler.showSystemContextMenu({
        x: Math.round(e.clientX),
        y: Math.round(e.clientY),
      });
    } else {
      this.allSectionsExpanded_ = this.$.list.areAllSectionsExpanded();
      this.$.menu.showAtPosition({
        top: e.clientY,
        left: e.clientX,
      });
    }
  };

  protected accessor allSectionsExpanded_: boolean = false;
  protected accessor shortcut_: string = loadTimeData.getString('shortcutText');
  protected accessor searchQuery_: string = '';
  protected accessor sectionDelegates_:
      Array<OrganizerListSectionDelegate<unknown>> =
          this.getSectionDelegates_();

  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().start();
    window.addEventListener('focus', this.onWindowFocused_);
    window.addEventListener('contextmenu', this.onContextMenu_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('focus', this.onWindowFocused_);
    window.removeEventListener('contextmenu', this.onContextMenu_);
  }

  private getSectionDelegates_(): Array<OrganizerListSectionDelegate<unknown>> {
    const delegates: Array<OrganizerListSectionDelegate<unknown>> = [
      new OpenTabsDelegate(),
      ...(!loadTimeData.getBoolean('isIncognitoMode') ?
              [new RecentTabsDelegate()] :
              []),
      new TabGroupsDelegate(),
    ];
    if (loadTimeData.getBoolean('foreignTabsEnabled')) {
      delegates.push(new ForeignTabsDelegate());
    }
    return delegates;
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.searchQuery_ = e.detail;
  }

  protected isSearching_(): boolean {
    return this.searchQuery_.length > 0;
  }

  protected onExpandAllClick_() {
    this.$.menu.close();
    this.$.list.expandAll();
  }

  protected onCollapseAllClick_() {
    this.$.menu.close();
    this.$.list.collapseAll();
  }

  protected onCloseTabSearchClick_() {
    this.$.menu.close();
    this.browserProxy_.handler.closePanel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-panel-app': OrganizerPanelAppElement;
  }
}

customElements.define(OrganizerPanelAppElement.is, OrganizerPanelAppElement);
