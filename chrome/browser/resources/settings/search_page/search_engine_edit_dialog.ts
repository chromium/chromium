// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-edit-dialog' is a component for adding
 * or editing a search engine entry.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './search_engine_edit_dialog.css.js';
import {getHtml} from './search_engine_edit_dialog.html.js';
import type {CategorizedTemplateUrls, SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo} from './search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

// The `id` to use when a new search engine is added.  See
// `kInvalidTemplateURLID`.
const DEFAULT_MODEL_ID: number = 0;

export interface SettingsSearchEngineEditDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancel: CrButtonElement,
    dialog: CrDialogElement,
    keyword: CrInputElement,
    queryUrl: CrInputElement,
    searchEngine: CrInputElement,
  };
}

export type SearchEngineEditDialogElement =
    SettingsSearchEngineEditDialogElement;

const SettingsSearchEngineEditDialogElementBase =
    WebUiListenerMixinLit(CrLitElement);

export class SettingsSearchEngineEditDialogElement extends
    SettingsSearchEngineEditDialogElementBase {
  static get is() {
    return 'settings-search-engine-edit-dialog';
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
       * The search engine to be edited. If not populated a new search engine
       * should be added.
       */
      model: {type: Object},
      searchEngine_: {type: String},
      keyword_: {type: String},
      queryUrl_: {type: String},
      dialogTitle_: {type: String},
      actionButtonText_: {type: String},
      showPolicySubtitle_: {type: Boolean},
      readonly_: {type: Boolean},
      urlIsReadonly_: {type: Boolean},
    };
  }

  accessor model: SearchEngine|null = null;
  protected accessor searchEngine_: string = '';
  protected accessor keyword_: string = '';
  protected accessor queryUrl_: string = '';
  protected accessor dialogTitle_: string = '';
  protected accessor actionButtonText_: string = '';
  protected accessor showPolicySubtitle_: boolean = false;
  protected accessor readonly_: boolean = false;
  protected accessor urlIsReadonly_: boolean = false;

  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'search-engines-changed',
        loadTimeData.getBoolean('searchSettingsUpdate') ?
            this.updateEnginesFromCategorizedUrls_.bind(this) :
            this.updateEnginesFromSearchEnginesInfo_.bind(this));

    this.browserProxy_.searchEngineEditStarted(
        this.model ? this.model.id : DEFAULT_MODEL_ID);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('model')) {
      if (this.model) {
        this.readonly_ = this.model.isManaged && !this.model.canBeEdited;
        if (this.model.isPrepopulated || this.model.default) {
          this.dialogTitle_ = loadTimeData.getString(
              this.readonly_ ? 'searchEnginesViewSearchEngine' :
                               'searchEnginesEditSearchEngine');
        } else {
          this.dialogTitle_ = loadTimeData.getString(
              this.readonly_ ? 'searchEnginesViewSiteSearch' :
                               'searchEnginesEditSiteSearch');
        }

        this.actionButtonText_ =
            loadTimeData.getString(this.readonly_ ? 'done' : 'save');
        this.showPolicySubtitle_ = this.model.isManaged;

        // If editing an existing search engine, pre-populate the input fields.
        this.searchEngine_ = this.model.name;
        this.keyword_ = this.model.keyword;
        this.queryUrl_ = this.model.url;
      } else {
        this.dialogTitle_ =
            loadTimeData.getString('searchEnginesAddSiteSearch');
        this.actionButtonText_ = loadTimeData.getString('add');
        this.readonly_ = false;
        this.showPolicySubtitle_ = false;
        this.searchEngine_ = '';
        this.keyword_ = '';
        this.queryUrl_ = '';
      }
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedProperties.has('model') ||
        changedPrivateProperties.has('readonly_')) {
      this.urlIsReadonly_ =
          this.readonly_ || (!!this.model && this.model.urlLocked);
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    this.addEventListener('cancel', () => {
      this.browserProxy_.searchEngineEditCancelled();
    });

    this.updateActionButtonState_();
    this.$.dialog.showModal();
  }

  protected onSearchEngineValueChanged_(e: CustomEvent<{value: string}>) {
    this.searchEngine_ = e.detail.value;
  }

  protected onSearchEngineInput_(e: Event) {
    this.validate_(e);
  }

  protected onKeywordValueChanged_(e: CustomEvent<{value: string}>) {
    this.keyword_ = e.detail.value;
  }

  protected onKeywordFocus_(e: Event) {
    this.validate_(e);
  }

  protected onKeywordInput_(e: Event) {
    this.validate_(e);
  }

  protected onQueryUrlValueChanged_(e: CustomEvent<{value: string}>) {
    this.queryUrl_ = e.detail.value;
  }

  protected onQueryUrlFocus_(e: Event) {
    this.validate_(e);
  }

  protected onQueryUrlInput_(e: Event) {
    this.validate_(e);
  }

  protected onCancelClick_() {
    this.cancel_();
  }

  private updateEnginesFromCategorizedUrls_(
      categorizedTemplateUrls: CategorizedTemplateUrls) {
    assert(loadTimeData.getBoolean('searchSettingsUpdate'));

    if (this.model) {
      const engineStillExists = [
        'activeSiteShortcuts',
        'inactiveSiteShortcuts',
        'activeFeatureShortcuts',
        'inactiveFeatureShortcuts',
      ].some(type => {
        return categorizedTemplateUrls[type].some(e => e.id === this.model!.id);
      });
      if (!engineStillExists) {
        this.cancel_();
        return;
      }
    }

    [this.$.searchEngine, this.$.keyword, this.$.queryUrl].forEach(
        element => this.validateElement_(element));
  }

  private updateEnginesFromSearchEnginesInfo_(
      searchEnginesInfo: SearchEnginesInfo) {
    assert(!loadTimeData.getBoolean('searchSettingsUpdate'));

    if (this.model) {
      const engineWasRemoved =
          ['defaults', 'actives', 'others', 'extensions'].every(
              engineType => searchEnginesInfo[engineType].every(
                  e => e.id !== this.model!.id));
      if (engineWasRemoved) {
        this.cancel_();
        return;
      }
    }

    [this.$.searchEngine, this.$.keyword, this.$.queryUrl].forEach(
        element => this.validateElement_(element));
  }

  protected cancel_() {
    this.$.dialog.cancel();
  }

  protected onActionButtonClick_() {
    this.browserProxy_.searchEngineEditCompleted(
        this.searchEngine_, this.keyword_, this.queryUrl_);
    this.$.dialog.close();
  }

  private validateElement_(inputElement: CrInputElement) {
    // No need to validate fields if the search engine is read-only, i.e.
    // created by policy. Those have been validated when the policy was
    // processed (b/348165485).
    if (this.readonly_) {
      return;
    }

    // If element is empty, disable the action button, but don't show the red
    // invalid message.
    if (inputElement.value === '') {
      inputElement.invalid = false;
      this.updateActionButtonState_();
      return;
    }

    this.browserProxy_
        .validateSearchEngineInput(inputElement.id, inputElement.value)
        .then(isValid => {
          inputElement.invalid = !isValid;
          this.updateActionButtonState_();
        });
  }

  private validate_(event: Event) {
    const inputElement = event.target as CrInputElement;
    this.validateElement_(inputElement);
  }

  private updateActionButtonState_() {
    const allValid = [
      this.$.searchEngine,
      this.$.keyword,
      this.$.queryUrl,
    ].every(function(inputElement) {
      return !inputElement.invalid && inputElement.value.length > 0;
    });
    this.$.actionButton.disabled = !allValid;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine-edit-dialog': SettingsSearchEngineEditDialogElement;
  }
}

customElements.define(
    SettingsSearchEngineEditDialogElement.is,
    SettingsSearchEngineEditDialogElement);
