// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScanSource, SourceType} from './scanning.mojom-webui.js';
import {alphabeticalCompare, getSourceTypeString} from './scanning_app_util.js';
import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';
import {getTemplate} from './source_select.html.js';

/** @type {SourceType} */
const DEFAULT_SOURCE_TYPE = SourceType.kFlatbed;

/**
 * @fileoverview
 * 'source-select' displays the available scanner sources in a dropdown.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {SelectBehaviorInterface}
 */
const SourceSelectElementBase =
    mixinBehaviors([I18nBehavior, SelectBehavior], PolymerElement);

/** @polymer */
class SourceSelectElement extends SourceSelectElementBase {
  static get is() {
    return 'source-select';
  }

  static get template() {
    return getTemplate();
  }

  /**
   * @param {number} index
   * @return {string}
   */
  getOptionAtIndex(index) {
    assert(index < this.options.length);

    return this.options[index].name;
  }

  /**
   * @param {SourceType} mojoSourceType
   * @return {string}
   * @private
   */
  getSourceTypeString_(mojoSourceType) {
    return getSourceTypeString(mojoSourceType);
  }

  sortOptions() {
    this.options.sort((a, b) => {
      return alphabeticalCompare(
          getSourceTypeString(a.type), getSourceTypeString(b.type));
    });
  }

  /**
   * @param {!ScanSource} option
   * @return {boolean}
   */
  isDefaultOption(option) {
    return option.type === DEFAULT_SOURCE_TYPE;
  }
}

customElements.define(SourceSelectElement.is, SourceSelectElement);
