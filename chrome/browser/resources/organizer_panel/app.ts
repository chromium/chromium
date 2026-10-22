// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import './organizer_list.js';
import '/strings.m.js';

import {assert} from '//resources/js/assert.js';
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
    dragRegion: HTMLElement,
    list: OrganizerListElement,
    searchField: CrToolbarSearchFieldElement,
    shortcut: HTMLElement,
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
      verticalTabsEnabled: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor verticalTabsEnabled: boolean = false;
  protected accessor shortcut_: string = loadTimeData.getString('shortcutText');
  protected accessor searchQuery_: string = '';
  protected accessor sectionDelegates_:
      Array<OrganizerListSectionDelegate<unknown>> =
          this.getSectionDelegates_();

  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private verticalTabsEnabledChangedListenerId_: number|null = null;
  private onKeydown_: (e: KeyboardEvent) => void = (e: KeyboardEvent) => {
    if (e.key === 'Tab') {
      const closeButton = this.getCloseButton_();
      if (closeButton && e.composedPath().includes(closeButton)) {
        if (!e.shiftKey) {
          this.$.searchField.getSearchInput().focus();
        } else {
          if (!this.$.list.focusLastSection()) {
            this.$.searchField.getSearchInput().focus();
          }
        }
        e.preventDefault();
        e.stopPropagation();
      }
    }
  };
  private onWindowFocused_: () => void = () => {
    // Autofocus only works when the window is initially shown; for subsequent
    // focus events, focus the search field.
    this.$.searchField.showAndFocus();
  };
  private onContextMenu_: (e: MouseEvent) => void = (e: MouseEvent) => {
    e.preventDefault();
  };
  private onVisibilityChange_: () => void = () => {
    if (document.visibilityState === 'hidden') {
      this.$.searchField.setValue('');
      this.searchQuery_ = '';
      this.$.list.scrollTop = 0;
    }
  };

  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().start();
    this.addEventListener('keydown', this.onKeydown_);
    window.addEventListener('focus', this.onWindowFocused_);
    window.addEventListener('contextmenu', this.onContextMenu_);
    document.addEventListener('visibilitychange', this.onVisibilityChange_);
    this.verticalTabsEnabledChangedListenerId_ =
        this.browserProxy_.callbackRouter.onVerticalTabsEnabledChanged
            .addListener((verticalTabsEnabled: boolean) => {
              this.verticalTabsEnabled = verticalTabsEnabled;
            });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeEventListener('keydown', this.onKeydown_);
    window.removeEventListener('focus', this.onWindowFocused_);
    window.removeEventListener('contextmenu', this.onContextMenu_);
    document.removeEventListener('visibilitychange', this.onVisibilityChange_);
    assert(this.verticalTabsEnabledChangedListenerId_ !== null);
    this.browserProxy_.callbackRouter.removeListener(
        this.verticalTabsEnabledChangedListenerId_);
    this.verticalTabsEnabledChangedListenerId_ = null;
  }

  private getCloseButton_(): HTMLElement|null {
    const button = this.shadowRoot?.querySelector<HTMLElement>('#closeButton');
    if (!button || button.hidden || button.style.display === 'none') {
      return null;
    }
    return button;
  }

  protected onFocusAfterList_(e: CustomEvent<{sourceEvent?: KeyboardEvent}>) {
    const closeButton = this.getCloseButton_();
    if (closeButton) {
      closeButton.focus();
      e.detail?.sourceEvent?.preventDefault();
      e.detail?.sourceEvent?.stopPropagation();
    }
  }

  protected onFocusBeforeList_(e: CustomEvent<{sourceEvent?: KeyboardEvent}>) {
    this.$.searchField.getSearchInput().focus();
    e.detail?.sourceEvent?.preventDefault();
    e.detail?.sourceEvent?.stopPropagation();
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

  protected onSearchIconClicked_() {
    this.$.searchField.getSearchInput().focus();
  }

  protected onShortcutClick_() {
    this.$.searchField.getSearchInput().focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-panel-app': OrganizerPanelAppElement;
  }
}

customElements.define(OrganizerPanelAppElement.is, OrganizerPanelAppElement);
