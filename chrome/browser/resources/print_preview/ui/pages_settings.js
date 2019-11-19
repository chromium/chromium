// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './print_preview_shared_css.js';
import './settings_section.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {areRangesEqual} from '../print_preview_utils.js';

import {InputBehavior} from './input_behavior.js';
import {SelectBehavior} from './select_behavior.js';
import {SettingsBehavior} from './settings_behavior.js';

/** @enum {number} */
const PagesInputErrorState = {
  NO_ERROR: 0,
  INVALID_SYNTAX: 1,
  OUT_OF_BOUNDS: 2,
  EMPTY: 3,
};

/** @enum {number} */
const PagesValue = {
  ALL: 0,
  CUSTOM: 1,
};

/**
 * Used in place of Number.parseInt(), to ensure values like '1  2' or '1a2' are
 * not allowed.
 * @param {string} value The value to convert to a number.
 * @return {number} The value converted to a number, or NaN if it cannot be
 *     converted.
 * @private
 */
function parseIntStrict(value) {
  if (/^\d+$/.test(value.trim())) {
    return Number(value);
  }
  return NaN;
}

Polymer({
  is: 'print-preview-pages-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior, InputBehavior, SelectBehavior],

  properties: {
    disabled: Boolean,

    pageCount: {
      type: Number,
      observer: 'onPageCountChange_',
    },

    /** @private {boolean} */
    controlsDisabled_: {
      type: Boolean,
      computed: 'computeControlsDisabled_(disabled, hasError_)',
    },

    /** @private {boolean} */
    customSelected_: {
      type: Boolean,
      value: false,
      observer: 'onCustomSelectedChange_',
    },

    /** @private {number} */
    errorState_: {
      type: Number,
      reflectToAttribute: true,
      value: PagesInputErrorState.NO_ERROR,
    },

    /** @private {string} */
    inputString_: {
      type: String,
      value: '',
    },

    /** @private {!Array<number>} */
    pagesToPrint_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @private {!Array<{to: number, from: number}>} */
    rangesToPrint_: {
      type: Array,
      computed: 'computeRangesToPrint_(pagesToPrint_)',
    },

    /**
     * Mirroring the enum so that it can be used from HTML bindings.
     * @private
     */
    pagesValueEnum_: {
      type: Object,
      value: PagesValue,
    },
  },

  observers: [
    'updatePagesToPrint_(inputString_)',
    'onRangeChange_(errorState_, rangesToPrint_, settings.pages, ' +
        'settings.pagesPerSheet.value)',
  ],

  listeners: {
    'input-change': 'onInputChange_',
  },

  /**
   * True if the user's last valid input should be restored to the custom input
   * field. Cleared when the input is set automatically, or the user manually
   * clears the field.
   * @private {boolean}
   */
  restoreLastInput_: true,

  /**
   * Initialize |selectedValue| in attached() since this doesn't observe
   * settings.pages, because settings.pages is not sticky.
   * @override
   */
  attached: function() {
    this.selectedValue = PagesValue.ALL.toString();
  },

  /** @return {!CrInputElement} The cr-input field element for InputBehavior. */
  getInput: function() {
    return /** @type {!CrInputElement} */ (this.$.pageSettingsCustomInput);
  },

  /**
   * @param {!CustomEvent<string>} e Contains the new input value.
   * @private
   */
  onInputChange_: function(e) {
    if (this.inputString_ !== e.detail) {
      this.restoreLastInput_ = true;
    }
    this.inputString_ = e.detail;
  },

  onProcessSelectChange: function(value) {
    this.customSelected_ = value === PagesValue.CUSTOM.toString();
  },

  /** @private */
  onCollapseChanged_: function() {
    if (this.customSelected_) {
      /** @type {!CrInputElement} */ (this.$.pageSettingsCustomInput)
          .inputElement.focus();
    }
  },

  /**
   * @return {boolean} Whether the controls should be disabled.
   * @private
   */
  computeControlsDisabled_: function() {
    // Disable the input if other settings are responsible for the error state.
    return !this.hasError_ && this.disabled;
  },

  /**
   * Updates pages to print and error state based on the validity and
   * current value of the input.
   * @private
   */
  updatePagesToPrint_: function() {
    if (!this.customSelected_) {
      this.errorState_ = PagesInputErrorState.NO_ERROR;
      this.pagesToPrint_ = this.pageCount ?
          Array.from(new Array(this.pageCount).fill(0), (_, i) => i + 1) :
          [];
      return;
    } else if (this.inputString_ === '') {
      this.errorState_ = PagesInputErrorState.EMPTY;
      return;
    }

    const pages = [];
    const added = {};
    const ranges = this.inputString_.split(/,|\u3001/);
    const maxPage = this.pageCount;
    for (const range of ranges) {
      if (range == '') {
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

      let min = parseIntStrict(limits[0]);
      if ((limits[0].length > 0 && Number.isNaN(min)) || min < 1) {
        this.errorState_ = PagesInputErrorState.INVALID_SYNTAX;
        this.onRangeChange_();
        return;
      }
      if (limits.length == 1) {
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

      let max = parseIntStrict(limits[1]);
      if (Number.isNaN(max) && limits[1].length > 0) {
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
  },

  /**
   * @return {!Array<{to: number, from: number}>}
   * @private
   */
  computeRangesToPrint_: function() {
    if (!this.pagesToPrint_ || this.pagesToPrint_.length == 0 ||
        this.pagesToPrint_[0] == -1 ||
        this.pagesToPrint_.length == this.pageCount) {
      return [];
    }

    let from = this.pagesToPrint_[0];
    let to = this.pagesToPrint_[0];
    const ranges = [];
    for (const page of this.pagesToPrint_.slice(1)) {
      if (page == to + 1) {
        to = page;
        continue;
      }
      ranges.push({from: from, to: to});
      from = page;
      to = page;
    }
    ranges.push({from: from, to: to});
    return ranges;
  },

  /**
   * @return {!Array<number>} The final page numbers, reflecting N-up setting.
   *     Page numbers are 1 indexed, since these numbers are displayed to the
   *     user.
   * @private
   */
  getNupPages_: function() {
    const pagesPerSheet =
        /** @type {number} */ (this.getSettingValue('pagesPerSheet'));
    if (pagesPerSheet <= 1 || this.pagesToPrint_.length == 0) {
      return this.pagesToPrint_;
    }

    const numPages = Math.ceil(this.pagesToPrint_.length / pagesPerSheet);
    const nupPages = new Array(numPages);
    for (let i = 0; i < nupPages.length; i++) {
      nupPages[i] = i + 1;
    }
    return nupPages;
  },

  /**
   * Updates the model with pages and validity, and adds error styling if
   * needed.
   * @private
   */
  onRangeChange_: function() {
    if (this.settings === undefined || this.pagesToPrint_ === undefined) {
      return;
    }

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
        this.rangesToPrint_,
        /** @type {!Array} */ (this.getSettingValue('ranges')));
    if (rangesChanged ||
        nupPages.length != this.getSettingValue('pages').length) {
      this.setSetting('pages', nupPages);
    }
    if (rangesChanged) {
      this.setSetting('ranges', this.rangesToPrint_);
    }
    this.setSettingValid('pages', true);
    this.hasError_ = false;
  },

  /** @private */
  onSelectBlur_: function(event) {
    if (!this.customSelected_ ||
        event.relatedTarget === this.$.pageSettingsCustomInput) {
      return;
    }

    this.onCustomInputBlur_();
  },

  /** @private */
  onCustomInputBlur_: function() {
    this.resetAndUpdate();
    if (this.errorState_ === PagesInputErrorState.EMPTY) {
      // Update with all pages.
      this.$$('cr-input').value = this.getAllPagesString_();
      this.inputString_ = this.getAllPagesString_();
      this.resetString();
      this.restoreLastInput_ = false;
    }
  },

  /**
   * @return {string} Gets message to show as hint.
   * @private
   */
  getHintMessage_: function() {
    if (this.errorState_ == PagesInputErrorState.NO_ERROR ||
        this.errorState_ == PagesInputErrorState.EMPTY) {
      return '';
    }

    let formattedMessage = '';
    if (this.errorState_ == PagesInputErrorState.INVALID_SYNTAX) {
      formattedMessage = loadTimeData.getStringF(
          'pageRangeSyntaxInstruction',
          loadTimeData.getString('examplePageRangeText'));
    } else {
      formattedMessage = loadTimeData.getStringF(
          'pageRangeLimitInstructionWithValue', this.pageCount);
    }
    return formattedMessage.replace(/<\/b>|<b>/g, '');
  },

  /**
   * @return {boolean} Whether to hide the hint.
   * @private
   */
  hintHidden_: function() {
    return this.errorState_ == PagesInputErrorState.NO_ERROR ||
        this.errorState_ == PagesInputErrorState.EMPTY;
  },

  /**
   * @return {boolean} Whether to disable the custom input.
   * @private
   */
  inputDisabled_: function() {
    return !this.customSelected_ || this.controlsDisabled_;
  },

  /**
   * @return {string} A string representing the full page range.
   * @private
   */
  getAllPagesString_: function() {
    if (this.pageCount === 0) {
      return '';
    }

    return this.pageCount === 1 ? '1' : `1-${this.pageCount}`;
  },

  /** @private */
  onCustomSelectedChange_: function() {
    if ((this.customSelected_ && !this.restoreLastInput_) ||
        this.errorState_ !== PagesInputErrorState.NO_ERROR) {
      this.restoreLastInput_ = true;
      this.inputString_ = '';
      this.$$('cr-input').value = '';
      this.resetString();
    }
    this.updatePagesToPrint_();
  },

  /**
   * @param {number} current
   * @param {number} previous
   * @private
   */
  onPageCountChange_: function(current, previous) {
    // Reset the custom input to the new "all pages" value if it is equal to the
    // full page range and was either set automatically, or would become invalid
    // due to the page count change.
    const resetCustom = this.customSelected_ && !!this.pagesToPrint_ &&
        this.pagesToPrint_.length === previous &&
        (current < previous || !this.restoreLastInput_);

    if (resetCustom) {
      this.$$('cr-input').value = this.getAllPagesString_();
      this.inputString_ = this.getAllPagesString_();
      this.resetString();
    } else {
      this.updatePagesToPrint_();
    }
  },
});
