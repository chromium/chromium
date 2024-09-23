// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the zero state for sea pen
 * images element when the user just selects a template.
 */

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sea_pen_zero_state_svg_element.html.js';

const SeaPenZeroStateSvgElementBase = I18nMixin(PolymerElement);

export class SeaPenZeroStateSvgElement extends SeaPenZeroStateSvgElementBase {
  static get is() {
    return 'sea-pen-zero-state-svg';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(SeaPenZeroStateSvgElement.is, SeaPenZeroStateSvgElement);
