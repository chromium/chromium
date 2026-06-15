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
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './search_engines_list.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';

import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './feature_shortcuts_page.html.js';
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
    I18nMixin(WebUiListenerMixin(PolymerElement));

export class FeatureShortcutsPageElement extends
    FeatureShortcutsPageElementBase {
  static get is() {
    return 'settings-feature-shortcuts-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activeShortcuts: Array,
      inactiveShortcuts: Array,

      // Whether the corresponding subsection is expanded.
      activeShortcutsExpanded_: {type: Boolean, value: false},
      inactiveShortcutsExpanded_: {type: Boolean, value: false},
    };
  }

  declare activeShortcuts: SearchEngine[];
  declare inactiveShortcuts: SearchEngine[];

  declare private activeShortcutsExpanded_: boolean;
  declare private inactiveShortcutsExpanded_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    SearchEnginesBrowserProxyImpl.getInstance()
        .getCategorizedTemplateUrls()
        .then(this.enginesChanged_.bind(this));
    this.addWebUiListener(
        'search-engines-changed', this.enginesChanged_.bind(this));
  }

  private enginesChanged_(categorizedTemplateUrls: CategorizedTemplateUrls) {
    this.activeShortcuts = categorizedTemplateUrls.activeFeatureShortcuts;
    this.inactiveShortcuts = categorizedTemplateUrls.inactiveFeatureShortcuts;
  }

  private onSectionExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    const interaction = e.detail.value ?
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
