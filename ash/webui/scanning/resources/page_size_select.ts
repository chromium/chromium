// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './page_size_select.html.js';
import {PageSize} from './scanning.mojom-webui.js';
import {alphabeticalCompare, getPageSizeString} from './scanning_app_util.js';
import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';

/** @type {PageSize} */
const DEFAULT_PAGE_SIZE = PageSize.kNaLetter;

/**
 * @fileoverview
 * 'page-size-select' displays the available page sizes in a dropdown.
 */

// TODO(b/300484132): Replace mixinBehavior with mixin implementation once a
// Mixin version of SelectBehavior is available.
const PageSizeSelectElementBase =
    mixinBehaviors([I18nBehavior, SelectBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior & SelectBehaviorInterface};

class PageSizeSelectElement extends PageSizeSelectElementBase {
  static get is() {
    return 'page-size-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  getOptionAtIndex(index: number): string {
    assert(index < this.options.length);

    return this.options[index].toString();
  }

  getPageSizeAsString(pageSize: PageSize): string {
    return getPageSizeString(pageSize);
  }

  sortOptions(): void {
    this.options.sort((a: PageSize, b: PageSize) => {
      return alphabeticalCompare(getPageSizeString(a), getPageSizeString(b));
    });

    // If the fit to scan area option exists, move it to the end of the page
    // sizes array.
    const fitToScanAreaIndex =
        this.options.findIndex((pageSize: PageSize): boolean => {
          return pageSize === PageSize.kMax;
        });


    if (fitToScanAreaIndex !== -1) {
      this.options.push(this.options.splice(fitToScanAreaIndex, 1)[0]);
    }
  }

  isDefaultOption(option: PageSize): boolean {
    return option === DEFAULT_PAGE_SIZE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PageSizeSelectElement.is]: PageSizeSelectElement;
  }
}

customElements.define(PageSizeSelectElement.is, PageSizeSelectElement);
