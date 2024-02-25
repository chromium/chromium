// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import './advanced_settings_item.js';
import './print_preview_search_box.js';
import './print_preview_shared.css.js';
import './print_preview_vars.css.js';
import '../strings.m.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {removeHighlights} from 'chrome://resources/js/search_highlight_utils.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
import {MetricsContext, PrintSettingsUiBucket} from '../metrics.js';

import {getTemplate} from './advanced_settings_dialog.html.js';
import type {PrintPreviewSearchBoxElement} from './print_preview_search_box.js';
import {SettingsMixin} from './settings_mixin.js';

export interface PrintPreviewAdvancedSettingsDialogElement {
  $: {
    dialog: CrDialogElement,
    searchBox: PrintPreviewSearchBoxElement,
  };
}

const PrintPreviewAdvancedSettingsDialogElementBase =
    I18nMixin(SettingsMixin(PolymerElement));

export class PrintPreviewAdvancedSettingsDialogElement extends
    PrintPreviewAdvancedSettingsDialogElementBase {
  static get is() {
    return 'print-preview-advanced-settings-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      destination: Object,

      searchQuery_: {
        type: Object,
        value: null,
      },

      hasMatching_: {
        type: Boolean,
        notify: true,
        computed: 'computeHasMatching_(searchQuery_)',
      },
    };
  }

  destination: Destination;
  private searchQuery_: RegExp|null;
  private hasMatching_: boolean;
  private highlights_: HTMLElement[] = [];
  private bubbles_: Map<HTMLElement, number> = new Map();
  private metrics_: MetricsContext = MetricsContext.printSettingsUi();

  override ready() {
    super.ready();

    this.addEventListener('keydown', e => this.onKeydown_(e as KeyboardEvent));
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
  private hasMultipleItems_(): boolean {
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

    const listItems = this.shadowRoot!.querySelectorAll(
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
  private shouldShowHint_(): boolean {
    return !!this.searchQuery_ && !this.hasMatching_;
  }

  private onCloseOrCancel_() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
    if (this.$.dialog.getNative().returnValue === 'success') {
      this.metrics_.record(
          PrintSettingsUiBucket.ADVANCED_SETTINGS_DIALOG_CANCELED);
    }
  }

  private onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  private onApplyButtonClick_() {
    const settingsValues: {[settingName: string]: any} = {};
    const itemList = this.shadowRoot!.querySelectorAll(
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

  private isSearching_(): string {
    return this.searchQuery_ ? 'searching' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-advanced-settings-dialog':
        PrintPreviewAdvancedSettingsDialogElement;
  }
}

customElements.define(
    PrintPreviewAdvancedSettingsDialogElement.is,
    PrintPreviewAdvancedSettingsDialogElement);
