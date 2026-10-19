// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-search-engine-list-dialog' is the dialog shown for displaying the
 * list of search engines from which the user can choose a default.
 *
 * When the "More options" entry point is enabled, clicking it fires a
 * 'more-engines-click' event, letting the parent replace this dialog with
 * 'settings-search-engine-more-engines-dialog'.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import './search_engine_icon.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {isRTL} from 'chrome://resources/js/util.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './search_engine_list_dialog.css.js';
import {getHtml} from './search_engine_list_dialog.html.js';
import type {SearchEngine, SearchEnginesBrowserProxy} from './search_engines_browser_proxy.js';
import {ChoiceMadeLocation, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

export interface SettingsSearchEngineListDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsSearchEngineListDialogElementBase =
    WebUiListenerMixinLit(CrLitElement);

export class SettingsSearchEngineListDialogElement extends
    SettingsSearchEngineListDialogElementBase {
  static get is() {
    return 'settings-search-engine-list-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * List of search engines available for the user to select as their
       * default search provider in the choice dialog (includes prepopulated
       * regional, default, managed policy, and recommended policy search
       * engines; excludes custom user-added engines).
       */
      searchEngines: {type: Array},

      /**
       * The id of the search engine that is selected by the user.
       */
      selectedEngineId_: {type: String},

      /**
       * State of the checkbox to save the search engine choice in guest mode,
       * or null if the checkbox is not displayed. Initialized by the parent,
       * then updated by the checkbox, firing 'save-guest-choice-changed'.
       */
      saveGuestChoice: {
        type: Boolean,
        notify: true,
      },

      /**
       * Whether the "More options" entry point is enabled via feature flag.
       */
      pickerWithMoreEnginesEnabled_: {type: Boolean},
    };
  }

  accessor searchEngines: SearchEngine[] = [];
  protected accessor selectedEngineId_: string = '';
  accessor saveGuestChoice: boolean|null = null;
  protected accessor pickerWithMoreEnginesEnabled_: boolean =
      loadTimeData.getBoolean('pickerWithMoreEngines');

  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('searchEngines')) {
      this.searchEnginesChanged_();
    }
  }

  override firstUpdated() {
    // The title is in the header slot rather than the title slot, so that the
    // "More options" button can be next to it without being in the <h2>.
    this.$.dialog.setTitleAriaLabel(this.getTitle_());
  }

  protected getTitle_(): string {
    return loadTimeData.getString(
        this.pickerWithMoreEnginesEnabled_ ? 'searchEnginesPickerTitle' :
                                             'searchPageTitle');
  }

  protected getSubtitle_(): string {
    return loadTimeData.getString(
        this.pickerWithMoreEnginesEnabled_ ?
            'searchEnginesPickerSubtitle' :
            'searchEnginesSettingsDialogSubtitle');
  }

  protected onMoreEnginesClick_() {
    this.fire('more-engines-click');
  }

  protected onSetAsDefaultClick_() {
    const searchEngine =
        this.searchEngines.find(engine => engine.id === this.selectedEngineId_);
    assert(searchEngine);

    this.browserProxy_.setDefaultSearchEngine(
        searchEngine.id, ChoiceMadeLocation.SEARCH_SETTINGS,
        this.saveGuestChoice);

    this.fire('search-engine-changed', {searchEngine});
    this.$.dialog.close();
  }

  protected onCancelClick_() {
    this.$.dialog.close();
  }

  protected onDialogCancel_() {
    this.$.dialog.close();
  }

  private searchEnginesChanged_() {
    if (!this.searchEngines.length) {
      this.selectedEngineId_ = '';
      return;
    }

    const defaultSearchEngine =
        this.searchEngines.find(searchEngine => searchEngine.default);
    assert(defaultSearchEngine);
    this.selectedEngineId_ = defaultSearchEngine.id;
  }

  protected onRadioGroupSelectedChanged_(e: CustomEvent<{value: string}>) {
    this.selectedEngineId_ = e.detail.value;
  }

  protected onSaveGuestChoiceCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.saveGuestChoice = e.detail.value;
  }

  protected getPolicyIndicatorTooltipPosition_(): string {
    return isRTL() ? 'right' : 'left';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine-list-dialog': SettingsSearchEngineListDialogElement;
  }
}

customElements.define(
    SettingsSearchEngineListDialogElement.is,
    SettingsSearchEngineListDialogElement);
