// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_shared.css.js';
import './strings.m.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './action_toolbar.html.js';

/**
 * @fileoverview
 * 'action-toolbar' is a floating toolbar that contains post-scan page options.
 */

const ActionToolbarElementBase = I18nMixin(PolymerElement);

export class ActionToolbarElement extends ActionToolbarElementBase {
  static get is() {
    return 'action-toolbar' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pageIndex: Number,

      numTotalPages: Number,

      pageNumberText: {
        type: String,
        computed: 'computePageNumberText(pageIndex, numTotalPages)',
      },
    };
  }

  pageIndex: number = 0;
  numTotalPages: number = 0;
  private pageNumberText: string = '';

  private computePageNumberText(): string {
    if (!this.numTotalPages || this.pageIndex >= this.numTotalPages) {
      return '';
    }

    assert(this.numTotalPages > 0);
    // Add 1 to |pageIndex| to get the corresponding page number.
    return this.i18n(
        'actionToolbarPageCountText', this.pageIndex + 1, this.numTotalPages);
  }

  private onRemovePageIconClick(): void {
    this.dispatchEvent(new CustomEvent<number>(
        'show-remove-page-dialog', {detail: this.pageIndex}));
  }

  private onRescanPageIconClick(): void {
    this.dispatchEvent(new CustomEvent<number>(
        'show-rescan-page-dialog', {detail: this.pageIndex}));
  }
}

declare global {
  interface HTMLElementEventMap {
    'show-remove-page-dialog': CustomEvent<number>;
    'show-rescan-page-dialog': CustomEvent<number>;
  }

  interface HTMLElementTagNameMap {
    [ActionToolbarElement.is]: ActionToolbarElement;
  }
}

customElements.define(ActionToolbarElement.is, ActionToolbarElement);
