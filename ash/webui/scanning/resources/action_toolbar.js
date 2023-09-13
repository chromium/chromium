// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_shared_css.js';
import './strings.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'action-toolbar' is a floating toolbar that contains post-scan page options.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ActionToolbarElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class ActionToolbarElement extends ActionToolbarElementBase {
  static get is() {
    return 'action-toolbar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {number} */
      pageIndex: Number,

      /** @type {number} */
      numTotalPages: Number,

      /** @private {string} */
      pageNumberText_: {
        type: String,
        computed: 'computePageNumberText_(pageIndex, numTotalPages)',
      },
    };
  }

  /**
   * @return {string}
   * @private
   */
  computePageNumberText_() {
    if (!this.numTotalPages || this.pageIndex >= this.numTotalPages) {
      return '';
    }

    assert(this.numTotalPages > 0);
    // Add 1 to |pageIndex| to get the corresponding page number.
    return this.i18n(
        'actionToolbarPageCountText', this.pageIndex + 1, this.numTotalPages);
  }

  /** @private */
  onRemovePageIconClick_() {
    this.dispatchEvent(
        new CustomEvent('show-remove-page-dialog', {detail: this.pageIndex}));
  }

  /** @private */
  onRescanPageIconClick_() {
    this.dispatchEvent(
        new CustomEvent('show-rescan-page-dialog', {detail: this.pageIndex}));
  }
}

customElements.define(ActionToolbarElement.is, ActionToolbarElement);
