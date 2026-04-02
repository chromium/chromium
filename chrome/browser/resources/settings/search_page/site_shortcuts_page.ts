// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-shortcuts-page' is the settings page containing site
 * shortcuts.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './search_engine_edit_dialog.js';
import './search_engines_list.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';

import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CategorizedTemplateUrls, SearchEngine, SearchEnginesBrowserProxy} from './search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';
import type {SettingsSearchEnginesListElement} from './search_engines_list.js';
import {getTemplate} from './site_shortcuts_page.html.js';

type SearchEngineEditEvent = CustomEvent<{
  engine: SearchEngine,
  anchorElement: HTMLElement,
}>;

type SearchEngineDeleteEvent = CustomEvent<{
  engine: SearchEngine,
  anchorElement: HTMLElement,
}>;

export interface SiteShortcutsPageElement {
  $: {
    activeShortcutsRow: CrExpandButtonElement,
    activeShortcutsList: SettingsSearchEnginesListElement,
    addSearchEngine: HTMLElement,
    inactiveShortcutsRow: CrExpandButtonElement,
    inactiveShortcutsList: SettingsSearchEnginesListElement,
    noActiveShortcutsFound: HTMLElement,
    noInactiveShortcutsFound: HTMLElement,
  };
}

const SiteShortcutsPageElementBase = WebUiListenerMixin(PolymerElement);

export class SiteShortcutsPageElement extends SiteShortcutsPageElementBase {
  static get is() {
    return 'settings-site-shortcuts-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activeShortcuts: {type: Array, value: []},
      inactiveShortcuts: {type: Array, value: []},

      // Whether the corresponding subsection is expanded.
      activeShortcutsExpanded_: {type: Boolean, value: false},
      inactiveShortcutsExpanded_: {type: Boolean, value: false},

      dialogModel_: {
        type: Object,
        value: null,
      },

      dialogAnchorElement_: {
        type: Object,
        value: null,
      },

      showEditDialog_: {
        type: Boolean,
        value: false,
      },

      showDeleteConfirmationDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  declare activeShortcuts: SearchEngine[];
  declare inactiveShortcuts: SearchEngine[];

  declare private activeShortcutsExpanded_: boolean;
  declare private inactiveShortcutsExpanded_: boolean;

  declare private dialogModel_: SearchEngine|null;
  declare private dialogAnchorElement_: HTMLElement|null;
  declare private showEditDialog_: boolean;
  declare private showDeleteConfirmationDialog_: boolean;

  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    SearchEnginesBrowserProxyImpl.getInstance()
        .getCategorizedTemplateUrls()
        .then(this.enginesChanged_.bind(this));
    this.addWebUiListener(
        'search-engines-changed', this.enginesChanged_.bind(this));

    this.addEventListener(
        'view-or-edit-search-engine',
        e => this.onEditSearchEngine_(e as SearchEngineEditEvent));
    this.addEventListener(
        'delete-search-engine',
        e => this.onDeleteSearchEngine_(e as SearchEngineDeleteEvent));
  }

  private enginesChanged_(categorizedTemplateUrls: CategorizedTemplateUrls) {
    this.activeShortcuts = categorizedTemplateUrls.activeSiteShortcuts;
    this.inactiveShortcuts = categorizedTemplateUrls.inactiveSiteShortcuts;
  }

  private openEditDialog_(
      searchEngine: SearchEngine|null, anchorElement: HTMLElement) {
    this.dialogModel_ = searchEngine;
    this.dialogAnchorElement_ = anchorElement;
    this.showEditDialog_ = true;
  }

  private onCloseEditDialog_() {
    this.showEditDialog_ = false;
    focusWithoutInk(this.dialogAnchorElement_ as HTMLElement);
    this.dialogModel_ = null;
    this.dialogAnchorElement_ = null;
  }

  private onEditSearchEngine_(e: SearchEngineEditEvent) {
    this.openEditDialog_(e.detail.engine, e.detail.anchorElement);
  }

  private openDeleteConfirmationDialog_(
      searchEngine: SearchEngine, anchorElement: HTMLElement) {
    this.dialogModel_ = searchEngine;
    this.dialogAnchorElement_ = anchorElement;
    this.showDeleteConfirmationDialog_ = true;
  }

  private onCloseDeleteConfirmationDialog_() {
    const dialog =
        this.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
    assert(dialog);
    const confirmed = dialog.wasConfirmed();
    this.showDeleteConfirmationDialog_ = false;

    if (confirmed) {
      assert(this.dialogModel_);
      const focusTarget = this.dialogModel_.canBeActivated ?
          this.$.inactiveShortcutsRow :
          this.$.activeShortcutsRow;

      this.browserProxy_.removeSearchEngine(this.dialogModel_.id);

      // If the engine is deleted, set the focus to the row that contained it.
      focusWithoutInk(focusTarget);
    } else {
      focusWithoutInk(this.dialogAnchorElement_ as HTMLElement);
    }

    this.dialogAnchorElement_ = null;
    this.dialogModel_ = null;
  }

  private onDeleteSearchEngine_(e: SearchEngineDeleteEvent) {
    this.openDeleteConfirmationDialog_(e.detail.engine, e.detail.anchorElement);
  }

  private onAddSearchEngineClick_(e: Event) {
    e.stopPropagation();
    this.openEditDialog_(null, this.$.addSearchEngine);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-site-shortcuts-page': SiteShortcutsPageElement;
  }
}

customElements.define(SiteShortcutsPageElement.is, SiteShortcutsPageElement);
