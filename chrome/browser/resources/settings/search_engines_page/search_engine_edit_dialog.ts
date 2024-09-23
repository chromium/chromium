// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-edit-dialog' is a component for adding
 * or editing a search engine entry.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './search_engine_edit_dialog.html.js';
import type {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo} from './search_engines_browser_proxy.js';
import {SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

/**
 * The |modelIndex| to use when a new search engine is added. Must match
 * with kNewSearchEngineIndex constant specified at
 * chrome/browser/ui/webui/settings/search_engines_handler.cc
 */
const DEFAULT_MODEL_INDEX: number = -1;

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

const SettingsSearchEngineEditDialogElementBase =
    WebUiListenerMixin(PolymerElement);

export class SettingsSearchEngineEditDialogElement extends
    SettingsSearchEngineEditDialogElementBase {
  static get is() {
    return 'settings-search-engine-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The search engine to be edited. If not populated a new search engine
       * should be added.
       */
      model: Object,

      searchEngine_: String,
      keyword_: String,
      queryUrl_: String,
      dialogTitle_: String,
      actionButtonText_: String,
      cancelButtonHidden_: Boolean,
      readonly_: Boolean,
      urlIsReadonly_: {
        type: Boolean,
        computed: 'computeUrlIsReadonly_(model, readonly_)',
      },
    };
  }

  model: SearchEngine|null;
  private searchEngine_: string;
  private keyword_: string;
  private queryUrl_: string;
  private dialogTitle_: string;
  private actionButtonText_: string;
  private cancelButtonHidden_: boolean;
  private readonly_: boolean;
  private urlIsReadonly_: boolean;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    if (this.model) {
      if (this.model.isPrepopulated || this.model.default) {
        this.dialogTitle_ =
            loadTimeData.getString('searchEnginesEditSearchEngine');
      } else {
        this.dialogTitle_ = loadTimeData.getString(
            this.model.isManaged ? 'searchEnginesViewSiteSearch' :
                                   'searchEnginesEditSiteSearch');
      }

      this.actionButtonText_ =
          loadTimeData.getString(this.model.isManaged ? 'done' : 'save');
      this.cancelButtonHidden_ = this.model.isManaged;

      // If editing an existing search engine, pre-populate the input fields.
      this.searchEngine_ = this.model.name;
      this.keyword_ = this.model.keyword;
      this.queryUrl_ = this.model.url;
      this.readonly_ = this.model.isManaged;
    } else {
      this.dialogTitle_ = loadTimeData.getString('searchEnginesAddSiteSearch');
      this.actionButtonText_ = loadTimeData.getString('add');
      this.readonly_ = false;
    }

    this.addEventListener('cancel', () => {
      this.browserProxy_.searchEngineEditCancelled();
    });

    this.addWebUiListener(
        'search-engines-changed', this.enginesChanged_.bind(this));
  }

  override connectedCallback() {
    super.connectedCallback();

    microTask.run(() => this.updateActionButtonState_());
    this.browserProxy_.searchEngineEditStarted(
        this.model ? this.model.modelIndex : DEFAULT_MODEL_INDEX);
    this.$.dialog.showModal();
  }

  private enginesChanged_(searchEnginesInfo: SearchEnginesInfo) {
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

  private cancel_() {
    this.$.dialog.cancel();
  }

  private onActionButtonClick_() {
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

  private computeUrlIsReadonly_(): boolean {
    return this.readonly_ || (!!this.model && this.model!.urlLocked);
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
