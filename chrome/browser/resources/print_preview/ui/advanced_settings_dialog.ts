// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './advanced_settings_item.js';
import './print_preview_search_box.js';
import '/strings.m.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {removeHighlights} from 'chrome://resources/js/search_highlight_utils.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {VendorCapability} from '../data/cdd.js';
import type {Destination} from '../data/destination.js';
import {MetricsContext, PrintSettingsUiBucket} from '../metrics.js';

import {getCss} from './advanced_settings_dialog.css.js';
import {getHtml} from './advanced_settings_dialog.html.js';
import type {PrintPreviewSearchBoxElement} from './print_preview_search_box.js';
import {SettingsMixin} from './settings_mixin.js';

export interface PrintPreviewAdvancedSettingsDialogElement {
  $: {
    dialog: CrDialogElement,
    searchBox: PrintPreviewSearchBoxElement,
  };
}

const PrintPreviewAdvancedSettingsDialogElementBase =
    I18nMixinLit(SettingsMixin(CrLitElement));

export class PrintPreviewAdvancedSettingsDialogElement extends
    PrintPreviewAdvancedSettingsDialogElementBase {
  static get is() {
    return 'print-preview-advanced-settings-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      destination: {type: Object},
      searchQuery_: {type: Object},

      hasMatching_: {
        type: Boolean,
        notify: true,
      },
    };
  }

  accessor destination: Destination|null = null;
  protected accessor searchQuery_: RegExp|null = null;
  private accessor hasMatching_: boolean = false;

  private highlights_: HTMLElement[] = [];
  private bubbles_: Map<HTMLElement, number> = new Map();
  private metrics_: MetricsContext = MetricsContext.printSettingsUi();

  override updated(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('searchQuery_')) {
      // Note: computeHasMatching_() updates the DOM in addition to calculating
      // the hasMatching_ value, so needs to be done in updated().
      this.hasMatching_ = this.computeHasMatching_();
    }
  }

  override firstUpdated() {
    this.addEventListener('keydown', e => this.onKeydown_(e));
  }

  override connectedCallback() {
    super.connectedCallback();

    this.metrics_.record(PrintSettingsUiBucket.ADVANCED_SETTINGS_DIALOG_SHOWN);
    this.$.dialog.showModal();
  }

  private onKeydown_(e: KeyboardEvent) {
    e.stopPropagation();
    const searchInput = this.$.searchBox.getSearchInput();
    const eventInSearchBox = e.composedPath().includes(searchInput);
    if (e.key === 'Escape' &&
        (!eventInSearchBox || !searchInput.value.trim())) {
      this.$.dialog.cancel();
      e.preventDefault();
      return;
    }

    if (e.key === 'Enter' && !eventInSearchBox) {
      const activeElementTag = (e.composedPath()[0] as HTMLElement).tagName;
      if (['CR-BUTTON', 'SELECT'].includes(activeElementTag)) {
        return;
      }
      this.onApplyButtonClick_();
      e.preventDefault();
      return;
    }
  }

  /**
   * @return Whether there is more than one vendor item to display.
   */
  protected hasMultipleItems_(): boolean {
    if (!this.destination) {
      return false;
    }

    return this.destination.capabilities!.printer.vendor_capability!.length > 1;
  }

  /**
   * @return Whether there is a setting matching the query.
   */
  private computeHasMatching_(): boolean {
    if (!this.shadowRoot) {
      return true;
    }

    removeHighlights(this.highlights_);
    this.bubbles_.forEach((_number, bubble) => bubble.remove());
    this.highlights_ = [];
    this.bubbles_.clear();

    const listItems = this.shadowRoot.querySelectorAll(
        'print-preview-advanced-settings-item');
    let hasMatch = false;
    listItems.forEach(item => {
      const matches = item.hasMatch(this.searchQuery_);
      item.hidden = !matches;
      hasMatch = hasMatch || matches;
      this.highlights_.push(
          ...item.updateHighlighting(this.searchQuery_, this.bubbles_));
    });
    return hasMatch;
  }

  /**
   * @return Whether the no matching settings hint should be shown.
   */
  protected shouldShowHint_(): boolean {
    return !!this.searchQuery_ && !this.hasMatching_;
  }

  protected onCloseOrCancel_() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
    if (this.$.dialog.getNative().returnValue === 'success') {
      this.metrics_.record(
          PrintSettingsUiBucket.ADVANCED_SETTINGS_DIALOG_CANCELED);
    }
  }

  protected onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  protected onApplyButtonClick_() {
    const settingsValues: {[settingName: string]: any} = {};
    const itemList = this.shadowRoot.querySelectorAll(
        'print-preview-advanced-settings-item');
    itemList.forEach(item => {
      settingsValues[item.capability.id] = item.getCurrentValue();
    });
    this.setSetting('vendorItems', settingsValues);
    this.$.dialog.close();
  }

  close() {
    this.$.dialog.close();
  }

  protected isSearching_(): string {
    return this.searchQuery_ ? 'searching' : '';
  }

  protected getVendorCapabilities_(): VendorCapability[] {
    return this.destination?.capabilities?.printer.vendor_capability || [];
  }

  protected onSearchQueryChanged_(e: CustomEvent<{value: RegExp | null}>) {
    this.searchQuery_ = e.detail.value;
  }
}

export type AdvancedSettingsDialogElement =
    PrintPreviewAdvancedSettingsDialogElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-advanced-settings-dialog':
        PrintPreviewAdvancedSettingsDialogElement;
  }
}

customElements.define(
    PrintPreviewAdvancedSettingsDialogElement.is,
    PrintPreviewAdvancedSettingsDialogElement);
