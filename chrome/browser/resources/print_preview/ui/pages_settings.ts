// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import './print_preview_shared.css.js';
import './settings_section.js';
import '/strings.m.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Range} from '../print_preview_utils.js';
import {areRangesEqual} from '../print_preview_utils.js';

import {InputMixin} from './input_mixin.js';
import {getCss} from './pages_settings.css.js';
import {getHtml} from './pages_settings.html.js';
import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

enum PagesInputErrorState {
  NO_ERROR = 0,
  INVALID_SYNTAX = 1,
  OUT_OF_BOUNDS = 2,
  EMPTY = 3,
}

export enum PagesValue {
  ALL = 0,
  ODDS = 1,
  EVENS = 2,
  CUSTOM = 3,
}

/**
 * Used in place of Number.parseInt(), to ensure values like '1  2' or '1a2' are
 * not allowed.
 * @param value The value to convert to a number.
 * @return The value converted to a number, or NaN if it cannot be converted.
 */
function parseIntStrict(value: string): number {
  if (/^\d+$/.test(value.trim())) {
    return Number(value);
  }
  return NaN;
}


export interface PrintPreviewPagesSettingsElement {
  $: {
    pageSettingsCustomInput: CrInputElement,
  };
}

const PrintPreviewPagesSettingsElementBase =
    WebUiListenerMixinLit(InputMixin(SettingsMixin(SelectMixin(CrLitElement))));

export class PrintPreviewPagesSettingsElement extends
    PrintPreviewPagesSettingsElementBase {
  static get is() {
    return 'print-preview-pages-settings';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
      pageCount: {type: Number},
      controlsDisabled_: {type: Boolean},

      errorState_: {
        type: Number,
        reflect: true,
      },

      hasError_: {type: Boolean},
      inputString_: {type: String},
      pagesToPrint_: {type: Array},
      rangesToPrint_: {type: Array},
      selection_: {type: Number},
    };
  }

  accessor disabled: boolean = false;
  accessor pageCount: number = 0;
  protected accessor controlsDisabled_: boolean = false;
  private accessor errorState_: PagesInputErrorState =
      PagesInputErrorState.NO_ERROR;
  protected accessor hasError_: boolean = false;
  private accessor inputString_: string = '';
  private accessor pagesToPrint_: number[] = [];
  private accessor rangesToPrint_: Range[] = [];
  private accessor selection_: PagesValue = PagesValue.ALL;

  /**
   * True if the user's last valid input should be restored to the custom
   * input field. Cleared when the input is set automatically, or the user
   * manually clears the field.
   */
  private restoreLastInput_: boolean = true;

  /**
   * Memorizes the user's last non-custom pages setting. Used when
   * `PagesValue.ODDS` and `PagesValue.EVEN` become invalid due to a changed
   * page count.
   */
  private restorationValue_: PagesValue = PagesValue.ALL;

  override firstUpdated() {
    this.addEventListener('input-change', e => this.onInputChange_(e));
  }

  /**
   * Initialize |selectedValue| in connectedCallback() since this doesn't
   * observe settings.pages, because settings.pages is not sticky.
   */
  override connectedCallback() {
    super.connectedCallback();

    this.selectedValue = PagesValue.ALL.toString();

    this.addSettingObserver('pages', () => this.onRangeChange_());
    this.addSettingObserver('pagesPerSheet.value', () => this.onRangeChange_());
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('disabled') ||
        changedPrivateProperties.has('hasError_')) {
      this.controlsDisabled_ = this.computeControlsDisabled_();
    }

    if (changedPrivateProperties.has('inputString_')) {
      this.updatePagesToPrint_();
    }

    if (changedPrivateProperties.has('pagesToPrint_')) {
      this.rangesToPrint_ = this.computeRangesToPrint_();
    }

    if (changedPrivateProperties.has('errorState_') ||
        changedPrivateProperties.has('rangesToPrint_')) {
      this.onRangeChange_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('pageCount')) {
      this.onPageCountChange_(
          this.pageCount, changedProperties.get('pageCount'));
    }

    if (changedPrivateProperties.has('selection_')) {
      this.onSelectionChange_();
    }
  }

  /** The cr-input field element for InputMixin. */
  override getInput() {
    return this.$.pageSettingsCustomInput;
  }

  private setSelectedValue_(value: PagesValue) {
    this.selectedValue = value.toString();
    this.shadowRoot.querySelector('select')!.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
  }

  private onInputChange_(e: CustomEvent<string>) {
    if (this.inputString_ !== e.detail) {
      this.restoreLastInput_ = true;
    }
    this.inputString_ = e.detail;
  }

  override onProcessSelectChange(value: string) {
    this.selection_ = parseInt(value, 10);
  }

  protected onCollapseChanged_() {
    if (this.selection_ === PagesValue.CUSTOM) {
      this.$.pageSettingsCustomInput.inputElement.focus();
    }
  }

  /**
   * @return Whether the controls should be disabled.
   */
  private computeControlsDisabled_(): boolean {
    // Disable the input if other settings are responsible for the error state.
    return !this.hasError_ && this.disabled;
  }

  /**
   * Updates pages to print and error state based on the validity and
   * current value of the input.
   */
  private updatePagesToPrint_() {
    if (this.selection_ !== PagesValue.CUSTOM) {
      this.errorState_ = PagesInputErrorState.NO_ERROR;
      if (!this.pageCount) {
        this.pagesToPrint_ = [];
        return;
      }

      const first = this.selection_ === PagesValue.EVENS ? 2 : 1;
      const step = this.selection_ === PagesValue.ALL ? 1 : 2;
      assert(first === 1 || this.pageCount !== 1);

      const length = Math.floor(1 + (this.pageCount - first) / step);
      this.pagesToPrint_ = Array.from({length}, (_, i) => step * i + first);
      return;
    } else if (this.inputString_ === '') {
      this.errorState_ = PagesInputErrorState.EMPTY;
      return;
    }

    const pages = [];
    const added: {[page: number]: boolean} = {};
    const ranges = this.inputString_.split(/,|\u3001/);
    const maxPage = this.pageCount;
    for (const range of ranges) {
      if (range === '') {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return;
      }

      const limits = range.split('-');
      if (limits.length > 2) {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return;
      }

      let min = parseIntStrict(limits[0]!);
      if ((limits[0]!.length > 0 && Number.isNaN(min)) || min < 1) {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return;
      }
      if (limits.length === 1) {
        if (min > maxPage) {
          this.errorState_ = PagesInputErrorState.OUT_OF_BOUNDS;
          this.onRangeChange_();
          return;
        }
        if (!added.hasOwnProperty(min)) {
          pages.push(min);
          added[min] = true;
        }
        continue;
      }

      let max = parseIntStrict(limits[1]!);
      if (Number.isNaN(max) && limits[1]!.length > 0) {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return;
      }

      if (Number.isNaN(min)) {
        min = 1;
      }
      if (Number.isNaN(max)) {
        max = maxPage;
      }
      if (min > max) {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return;
      }
      if (max > maxPage) {
        this.errorState_ = PagesInputErrorState.OUT_OF_BOUNDS;
        this.onRangeChange_();
        return;
      }
      for (let i = min; i <= max; i++) {
        if (!added.hasOwnProperty(i)) {
          pages.push(i);
          added[i] = true;
        }
      }
    }

    // Page numbers should be sorted to match the order of the pages in the
    // rendered PDF.
    pages.sort((left, right) => left - right);

    this.errorState_ = PagesInputErrorState.NO_ERROR;
    this.pagesToPrint_ = pages;
  }

  private computeRangesToPrint_(): Range[] {
    if (!this.pagesToPrint_ || this.pagesToPrint_.length === 0 ||
        this.pagesToPrint_[0] === -1 ||
        this.pagesToPrint_.length === this.pageCount) {
      return [];
    }

    let from = this.pagesToPrint_[0]!;
    let to = this.pagesToPrint_[0]!;
    const ranges: Range[] = [];
    for (const page of this.pagesToPrint_.slice(1)) {
      if (page === to + 1) {
        to = page;
        continue;
      }
      ranges.push({from: from, to: to});
      from = page;
      to = page;
    }
    ranges.push({from: from, to: to});
    return ranges;
  }

  /**
   * @return The final page numbers, reflecting N-up setting.
   *     Page numbers are 1 indexed, since these numbers are displayed to the
   *     user.
   */
  private getNupPages_(): number[] {
    const pagesPerSheet = this.getSettingValue('pagesPerSheet') as number;
    if (pagesPerSheet <= 1 || this.pagesToPrint_.length === 0) {
      return this.pagesToPrint_;
    }

    const numPages = Math.ceil(this.pagesToPrint_.length / pagesPerSheet);
    const nupPages = new Array(numPages);
    for (let i = 0; i < nupPages.length; i++) {
      nupPages[i] = i + 1;
    }
    return nupPages;
  }

  /**
   * Updates the model with pages and validity, and adds error styling if
   * needed.
   */
  private onRangeChange_() {
    if (this.errorState_ === PagesInputErrorState.EMPTY) {
      this.setSettingValid('pages', true);
      this.hasError_ = false;
      return;
    }

    if (this.errorState_ !== PagesInputErrorState.NO_ERROR) {
      this.hasError_ = true;
      this.setSettingValid('pages', false);
      return;
    }

    const nupPages = this.getNupPages_();
    const rangesChanged = !areRangesEqual(
        this.rangesToPrint_, this.getSettingValue('ranges') as Range[]);
    if (rangesChanged ||
        nupPages.length !== this.getSettingValue('pages').length) {
      this.setSetting('pages', nupPages);
    }
    if (rangesChanged) {
      this.setSetting('ranges', this.rangesToPrint_);
    }
    this.setSettingValid('pages', true);
    this.hasError_ = false;
  }

  protected onSelectBlur_(event: FocusEvent) {
    if (this.selection_ !== PagesValue.CUSTOM ||
        event.relatedTarget === this.$.pageSettingsCustomInput) {
      return;
    }

    this.onCustomInputBlur_();
  }

  protected async onCustomInputBlur_() {
    this.resetAndUpdate();
    await this.shadowRoot.querySelector('cr-input')!.updateComplete;

    if (this.errorState_ === PagesInputErrorState.EMPTY) {
      // Update with all pages.
      this.shadowRoot.querySelector('cr-input')!.value =
          this.getAllPagesString_();
      this.inputString_ = this.getAllPagesString_();
      this.resetString();
      this.restoreLastInput_ = false;
    }
    this.dispatchEvent(new CustomEvent(
        'custom-input-blurred-for-test', {bubbles: true, composed: true}));
  }

  /**
   * @return Message to show as hint.
   */
  protected getHintMessage_(): string {
    if (this.errorState_ === PagesInputErrorState.NO_ERROR ||
        this.errorState_ === PagesInputErrorState.EMPTY) {
      return '';
    }

    let formattedMessage = '';
    if (this.errorState_ === PagesInputErrorState.INVALID_SYNTAX) {
      formattedMessage = loadTimeData.getStringF(
          'pageRangeSyntaxInstruction',
          loadTimeData.getString('examplePageRangeText'));
    } else {
      formattedMessage = loadTimeData.getStringF(
          'pageRangeLimitInstructionWithValue', this.pageCount);
    }
    return formattedMessage.replace(/<\/b>|<b>/g, '');
  }

  /**
   * @return Whether the document being printed has only one page.
   */
  protected isSinglePage_(): boolean {
    return this.pageCount === 1;
  }

  /**
   * @return Whether to disable the custom input.
   */
  protected inputDisabled_(): boolean {
    return this.selection_ !== PagesValue.CUSTOM || this.controlsDisabled_;
  }

  /**
   * @return Whether to display the custom input.
   */
  protected shouldShowInput_(): boolean {
    return this.selection_ === PagesValue.CUSTOM;
  }

  /**
   * @return A string representing the full page range.
   */
  private getAllPagesString_(): string {
    if (this.pageCount === 0) {
      return '';
    }

    return this.pageCount === 1 ? '1' : `1-${this.pageCount}`;
  }

  private onSelectionChange_() {
    const customSelected = this.selection_ === PagesValue.CUSTOM;
    if ((customSelected && !this.restoreLastInput_) ||
        this.errorState_ !== PagesInputErrorState.NO_ERROR) {
      this.restoreLastInput_ = true;
      this.inputString_ = '';
      this.shadowRoot.querySelector('cr-input')!.value = '';
      this.resetString();
    }
    this.updatePagesToPrint_();
  }

  private onPageCountChange_(current: number, previous: number|undefined) {
    // Remember non-custom page settings when the page count changes to 1, so
    // they can be re-applied if the page count exceeds 1 again.
    if (this.selection_ !== PagesValue.CUSTOM) {
      if (current === 1) {
        this.restorationValue_ = this.selection_;
        this.setSelectedValue_(PagesValue.ALL);
      } else if (previous === 1) {
        assert(this.restorationValue_ !== PagesValue.CUSTOM);
        this.setSelectedValue_(this.restorationValue_);
      }
    }

    // Reset the custom input to the new "all pages" value if it is equal to the
    // full page range and was either set automatically, or would become invalid
    // due to the page count change.
    const resetCustom = this.selection_ === PagesValue.CUSTOM &&
        !!this.pagesToPrint_ && this.pagesToPrint_.length === previous &&
        (current < previous || !this.restoreLastInput_);

    if (resetCustom) {
      this.shadowRoot.querySelector('cr-input')!.value =
          this.getAllPagesString_();
      this.inputString_ = this.getAllPagesString_();
      this.resetString();
    } else {
      this.updatePagesToPrint_();
    }
  }
}

export type PagesSettingsElement = PrintPreviewPagesSettingsElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-pages-settings': PrintPreviewPagesSettingsElement;
  }
}

customElements.define(
    PrintPreviewPagesSettingsElement.is, PrintPreviewPagesSettingsElement);
