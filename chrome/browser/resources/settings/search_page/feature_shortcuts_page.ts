// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-feature-shortcuts-page' is the settings page containing
 * feature and extension shortcuts.
 */
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './search_engines_list.js';
import '../settings_page/settings_section.js';

import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './feature_shortcuts_page.css.js';
import {getHtml} from './feature_shortcuts_page.html.js';
import type {CategorizedTemplateUrls, SearchEngine} from './search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl, SearchEnginesInteractions} from './search_engines_browser_proxy.js';
import type {SettingsSearchEnginesListElement} from './search_engines_list.js';

export interface FeatureShortcutsPageElement {
  $: {
    activeShortcutsList: SettingsSearchEnginesListElement,
    activeShortcutsRow: CrExpandButtonElement,
    inactiveShortcutsList: SettingsSearchEnginesListElement,
    inactiveShortcutsRow: CrExpandButtonElement,
    noActiveShortcutsFound: HTMLElement,
    noInactiveShortcutsFound: HTMLElement,
  };
}

const FeatureShortcutsPageElementBase =
    I18nMixinLit(WebUiListenerMixinLit(CrLitElement));

export class FeatureShortcutsPageElement extends
    FeatureShortcutsPageElementBase {
  static get is() {
    return 'settings-feature-shortcuts-page';
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
    };
  }

  accessor activeShortcuts: SearchEngine[] = [];
  accessor inactiveShortcuts: SearchEngine[] = [];

  protected accessor activeShortcutsExpanded_: boolean = false;
  protected accessor inactiveShortcutsExpanded_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    SearchEnginesBrowserProxyImpl.getInstance()
        .getCategorizedTemplateUrls()
        .then(this.enginesChanged_.bind(this));
    this.addWebUiListener(
        'search-engines-changed', this.enginesChanged_.bind(this));
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

  private enginesChanged_(categorizedTemplateUrls: CategorizedTemplateUrls) {
    this.activeShortcuts = categorizedTemplateUrls.activeFeatureShortcuts;
    this.inactiveShortcuts = categorizedTemplateUrls.inactiveFeatureShortcuts;
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
        SearchEnginesInteractions.FEATURE_SHORTCUTS_SECTION_EXPANDED :
        SearchEnginesInteractions.FEATURE_SHORTCUTS_SECTION_COLLAPSED;
    SearchEnginesBrowserProxyImpl.getInstance()
        .recordSearchEnginesPageHistogram(interaction);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-feature-shortcuts-page': FeatureShortcutsPageElement;
  }
}

customElements.define(
    FeatureShortcutsPageElement.is, FeatureShortcutsPageElement);
