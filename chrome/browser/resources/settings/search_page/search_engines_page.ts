// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-page' is the settings page
 * containing search engines settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../simple_confirmation_dialog.js';
import './search_engine_edit_dialog.js';
import './search_engines_list.js';
import './omnibox_extension_entry.js';
import '../settings_page/settings_subpage.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import type {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo} from './search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl, SearchEnginesInteractions} from './search_engines_browser_proxy.js';
import {getCss} from './search_engines_page.css.js';
import {getHtml} from './search_engines_page.html.js';

type SearchEngineEditEvent = CustomEvent<{
  engine: SearchEngine,
  anchorElement: HTMLElement,
}>;

type SearchEngineDeleteEvent = CustomEvent<{
  engine: SearchEngine,
  anchorElement: HTMLElement,
}>;

export interface SettingsSearchEnginesPageElement {
  $: {
    keyboardShortcutSettingGroup: SettingsRadioGroupElement,
  };
}

export type SearchEnginesPageElement = SettingsSearchEnginesPageElement;

const SettingsSearchEnginesPageElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement)));

export class SettingsSearchEnginesPageElement extends
    SettingsSearchEnginesPageElementBase {
  static get is() {
    return 'settings-search-engines-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      defaultEngines: {type: Array},
      activeEngines: {type: Array},
      otherEngines: {type: Array},
      extensions: {type: Array},

      showExtensionsList_: {type: Boolean},

      /** Filters out all search engines that do not match. */
      filter_: {type: String},

      matchingDefaultEngines_: {type: Array},
      matchingActiveEngines_: {type: Array},
      matchingOtherEngines_: {type: Array},
      matchingExtensions_: {type: Array},

      dialogModel_: {type: Object},
      dialogAnchorElement_: {type: Object},
      showEditDialog_: {type: Boolean},
      showDeleteConfirmationDialog_: {type: Boolean},
    };
  }

  accessor defaultEngines: SearchEngine[] = [];
  accessor activeEngines: SearchEngine[] = [];
  accessor otherEngines: SearchEngine[] = [];
  accessor extensions: SearchEngine[] = [];
  protected accessor showExtensionsList_: boolean = false;
  protected accessor filter_: string = '';
  protected accessor matchingDefaultEngines_: SearchEngine[] = [];
  protected accessor matchingActiveEngines_: SearchEngine[] = [];
  protected accessor matchingOtherEngines_: SearchEngine[] = [];
  protected accessor matchingExtensions_: SearchEngine[] = [];
  protected accessor dialogModel_: SearchEngine|null = null;
  protected accessor dialogAnchorElement_: HTMLElement|null = null;
  protected accessor showEditDialog_: boolean = false;
  protected accessor showDeleteConfirmationDialog_: boolean = false;

  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('defaultEngines') ||
        changedPrivateProperties.has('filter_')) {
      this.matchingDefaultEngines_ =
          this.computeMatchingEngines_(this.defaultEngines);
    }

    if (changedProperties.has('activeEngines') ||
        changedPrivateProperties.has('filter_')) {
      this.matchingActiveEngines_ =
          this.computeMatchingEngines_(this.activeEngines);
    }

    if (changedProperties.has('otherEngines') ||
        changedPrivateProperties.has('filter_')) {
      this.matchingOtherEngines_ =
          this.computeMatchingEngines_(this.otherEngines);
    }

    if (changedProperties.has('extensions') ||
        changedPrivateProperties.has('filter_')) {
      this.matchingExtensions_ = this.computeMatchingEngines_(this.extensions);
    }

    if (changedProperties.has('extensions')) {
      this.showExtensionsList_ = this.extensions.length > 0;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    this.browserProxy_.getSearchEnginesList().then(
        this.enginesChanged_.bind(this));
    this.addWebUiListener(
        'search-engines-changed', this.enginesChanged_.bind(this));

    this.addEventListener(
        'view-or-edit-search-engine',
        e => this.onEditSearchEngine_(e as SearchEngineEditEvent));

    this.addEventListener(
        'delete-search-engine',
        e => this.onDeleteSearchEngine_(e as SearchEngineDeleteEvent));
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    // Restore focus to the last element if the previous last element was
    // deleted while it was focused.
    if (changedPrivateProperties.has('matchingExtensions_')) {
      const previousExtensions = changedPrivateProperties.get(
                                     'matchingExtensions_') as SearchEngine[] |
          undefined;
      if (previousExtensions &&
          this.matchingExtensions_.length < previousExtensions.length) {
        const focused = this.shadowRoot.querySelector(
            'settings-omnibox-extension-entry:focus-within');
        if (!focused) {
          const toFocus = this.shadowRoot.querySelector<HTMLElement>(
              'settings-omnibox-extension-entry:last-of-type');
          toFocus?.focus();
        }
      }
    }
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.filter_ = e.detail;
  }

  private openEditDialog_(
      searchEngine: SearchEngine|null, anchorElement: HTMLElement) {
    this.dialogModel_ = searchEngine;
    this.dialogAnchorElement_ = anchorElement;
    this.showEditDialog_ = true;
  }

  private openDeleteConfirmationDialog_(
      searchEngine: SearchEngine|null, anchorElement: HTMLElement) {
    this.dialogModel_ = searchEngine;
    this.dialogAnchorElement_ = anchorElement;
    this.showDeleteConfirmationDialog_ = true;
  }

  protected getDeleteConfirmationBodyText_(searchEngine: SearchEngine|null):
      string {
    if (searchEngine && searchEngine.isManaged) {
      return this.i18n('searchEnginesDeleteConfirmationDescriptionForPolicy');
    }
    return this.i18n('searchEnginesDeleteConfirmationDescription');
  }

  protected onEditDialogClose_() {
    this.showEditDialog_ = false;
    focusWithoutInk(this.dialogAnchorElement_ as HTMLElement);
    this.dialogModel_ = null;
    this.dialogAnchorElement_ = null;
  }

  protected onDeleteConfirmationDialogClose_() {
    const dialog =
        this.shadowRoot.querySelector('settings-simple-confirmation-dialog');
    assert(dialog);
    const confirmed = dialog.wasConfirmed();
    this.showDeleteConfirmationDialog_ = false;

    if (confirmed) {
      assert(this.dialogModel_);
      this.browserProxy_.removeSearchEngine(this.dialogModel_.id);
      this.dialogAnchorElement_ = null;
    }

    this.dialogModel_ = null;
  }

  private onEditSearchEngine_(e: SearchEngineEditEvent) {
    this.openEditDialog_(e.detail.engine, e.detail.anchorElement);
  }

  private onDeleteSearchEngine_(e: SearchEngineDeleteEvent) {
    this.openDeleteConfirmationDialog_(e.detail.engine, e.detail.anchorElement);
  }

  private enginesChanged_(searchEnginesInfo: SearchEnginesInfo) {
    this.defaultEngines = searchEnginesInfo.defaults;
    this.activeEngines = searchEnginesInfo.actives;
    this.otherEngines = searchEnginesInfo.others;
    this.extensions = searchEnginesInfo.extensions;
  }

  protected onAddSearchEngineClick_(e: Event) {
    e.preventDefault();
    this.browserProxy_.recordSearchEnginesPageHistogram(
        SearchEnginesInteractions.ADD_SEARCH_ENGINE);
    this.openEditDialog_(
        null, this.shadowRoot.querySelector('#addSearchEngine')!);
  }

  /**
   * Filters the given list based on the currently existing filter string.
   */
  private computeMatchingEngines_(list: SearchEngine[]): SearchEngine[] {
    if (this.filter_ === '') {
      return list;
    }

    const filter = this.filter_.toLowerCase();
    return list.filter(e => {
      return [e.displayName, e.name, e.keyword, e.url].some(
          term => term.toLowerCase().includes(filter));
    });
  }

  /**
   * @param list The original list.
   * @param filteredList The filtered list.
   * @return Whether to show the "no results" message.
   */
  protected showNoResultsMessage_(
      list: SearchEngine[], filteredList: SearchEngine[]): boolean {
    return list.length > 0 && filteredList.length === 0;
  }

  protected onKeyboardShortcutSettingChange_() {
    const spaceEnabled =
        this.$.keyboardShortcutSettingGroup.selected === 'true';

    this.browserProxy_.recordSearchEnginesPageHistogram(
        spaceEnabled ?
            SearchEnginesInteractions.KEYBOARD_SHORTCUT_SPACE_OR_TAB :
            SearchEnginesInteractions.KEYBOARD_SHORTCUT_TAB);
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engines-page': SettingsSearchEnginesPageElement;
  }
}

customElements.define(
    SettingsSearchEnginesPageElement.is, SettingsSearchEnginesPageElement);
