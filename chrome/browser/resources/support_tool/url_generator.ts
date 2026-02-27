// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy, DataCollectorItem, SupportTokenGenerationResult} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './url_generator.css.js';
import {getHtml} from './url_generator.html.js';

export interface UrlGeneratorElement {
  $: {
    caseIdInput: CrInputElement,
    copyToast: CrToastElement,
    copyTokenButton: CrButtonElement,
    copyURLButton: CrButtonElement,
    errorMessageToast: CrToastElement,
  };
}

const UrlGeneratorElementBase = I18nMixinLit(CrLitElement);

export class UrlGeneratorElement extends UrlGeneratorElementBase {
  static get is() {
    return 'url-generator';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      caseId_: {type: String},
      dataCollectors_: {type: Array},
      errorMessage_: {type: String},
      buttonDisabled_: {type: Boolean},
      copiedToastMessage_: {type: String},
      selectAll_: {type: Boolean},
    };
  }

  protected accessor caseId_: string = '';
  private generatedResult_: string = '';
  protected accessor errorMessage_: string = '';
  protected accessor buttonDisabled_: boolean = true;
  protected accessor copiedToastMessage_: string = '';
  protected accessor dataCollectors_: DataCollectorItem[] = [];
  protected accessor selectAll_: boolean = false;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getAllDataCollectors().then(
        (dataCollectors: DataCollectorItem[]) => {
          this.dataCollectors_ = dataCollectors;
        });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('selectAll_')) {
      // Update this.dataCollectors_ to reflect the selection choice.
      for (const collector of this.dataCollectors_) {
        collector.isIncluded = this.selectAll_;
      }
    }

    // Calling this unconditionally, as the array may be modified in place.
    this.onDataCollectorItemChange_();
  }

  protected onDataCollectorItemChange_() {
    // The button should be disabled if no data collector is selected.
    this.buttonDisabled_ =
        !this.dataCollectors_.some(collector => collector.isIncluded);
  }

  private showErrorMessageToast_(errorMessage: string) {
    this.errorMessage_ = errorMessage;
    this.$.errorMessageToast.show();
  }

  private showGenerationResult(
      result: SupportTokenGenerationResult, toastMessage: string) {
    if (result.success) {
      this.generatedResult_ = result.token;
      navigator.clipboard.writeText(this.generatedResult_);
      this.copiedToastMessage_ = toastMessage;
      this.$.copyToast.show();
      this.$.copyToast.focus();
    } else {
      this.showErrorMessageToast_(result.errorMessage);
    }
  }

  protected onCaseIdInput_(e: Event) {
    this.caseId_ = (e.target as HTMLInputElement).value;
  }

  protected async onCopyUrlClick_() {
    const result = await this.browserProxy_.generateCustomizedUrl(
        this.caseId_, this.dataCollectors_);
    this.showGenerationResult(result, this.i18n('linkCopied'));
  }

  protected async onCopyTokenClick_() {
    const result =
        await this.browserProxy_.generateSupportToken(this.dataCollectors_);
    this.showGenerationResult(result, this.i18n('tokenCopied'));
  }

  protected onErrorMessageToastCloseClick_() {
    this.$.errorMessageToast.hide();
  }

  protected onSelectAllCheckboxCheckedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.selectAll_ = e.detail.value;
  }

  protected onDataCollectorCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    const index = Number((e.target as HTMLElement).dataset['index']);
    this.dataCollectors_[index]!.isIncluded = e.detail.value;
    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'url-generator': UrlGeneratorElement;
  }
}

customElements.define(UrlGeneratorElement.is, UrlGeneratorElement);
