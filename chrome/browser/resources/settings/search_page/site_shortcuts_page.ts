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
import './search_engine_edit_dialog.js';
import './search_engines_list.js';
import '../settings_page/settings_section.js';
import '../simple_confirmation_dialog.js';

import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CategorizedTemplateUrls, SearchEngine, SearchEnginesBrowserProxy} from './search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl, SearchEnginesInteractions} from './search_engines_browser_proxy.js';
import type {SettingsSearchEnginesListElement} from './search_engines_list.js';
import {getCss} from './site_shortcuts_page.css.js';
import {getHtml} from './site_shortcuts_page.html.js';

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

const SiteShortcutsPageElementBase = WebUiListenerMixinLit(CrLitElement);

export class SiteShortcutsPageElement extends SiteShortcutsPageElementBase {
  static get is() {
    return 'settings-site-shortcuts-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      activeShortcuts: {type: Array},
      inactiveShortcuts: {type: Array},

      // Whether the corresponding subsection is expanded.
      activeShortcutsExpanded_: {type: Boolean},
      inactiveShortcutsExpanded_: {type: Boolean},

      dialogModel_: {type: Object},
      dialogAnchorElement_: {type: Object},
      showEditDialog_: {type: Boolean},
      showDeleteConfirmationDialog_: {type: Boolean},
    };
  }

  accessor activeShortcuts: SearchEngine[] = [];
  accessor inactiveShortcuts: SearchEngine[] = [];

  protected accessor activeShortcutsExpanded_: boolean = false;
  protected accessor inactiveShortcutsExpanded_: boolean = false;

  protected accessor dialogModel_: SearchEngine|null = null;
  protected accessor dialogAnchorElement_: HTMLElement|null = null;
  protected accessor showEditDialog_: boolean = false;
  protected accessor showDeleteConfirmationDialog_: boolean = false;

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

  protected onEditDialogClose_() {
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

  protected onDeleteConfirmationDialogClose_() {
    const dialog =
        this.shadowRoot.querySelector('settings-simple-confirmation-dialog');
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

  protected onAddSearchEngineClick_(e: Event) {
    e.stopPropagation();
    this.browserProxy_.recordSearchEnginesPageHistogram(
        SearchEnginesInteractions.ADD_SEARCH_ENGINE);
    this.openEditDialog_(null, this.$.addSearchEngine);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('activeShortcutsExpanded_') &&
        changedPrivateProperties.get('activeShortcutsExpanded_') !==
            undefined) {
      this.onSectionExpandedChanged_(this.activeShortcutsExpanded_);
    }

    if (changedPrivateProperties.has('inactiveShortcutsExpanded_') &&
        changedPrivateProperties.get('inactiveShortcutsExpanded_') !==
            undefined) {
      this.onSectionExpandedChanged_(this.inactiveShortcutsExpanded_);
    }
  }

  protected onActiveShortcutsExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.activeShortcutsExpanded_ = e.detail.value;
  }

  protected onInactiveShortcutsExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.inactiveShortcutsExpanded_ = e.detail.value;
  }

  private onSectionExpandedChanged_(expanded: boolean) {
    const interaction = expanded ?
        SearchEnginesInteractions.SITE_SHORTCUTS_SECTION_EXPANDED :
        SearchEnginesInteractions.SITE_SHORTCUTS_SECTION_COLLAPSED;
    this.browserProxy_.recordSearchEnginesPageHistogram(interaction);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-site-shortcuts-page': SiteShortcutsPageElement;
  }
}

customElements.define(SiteShortcutsPageElement.is, SiteShortcutsPageElement);
