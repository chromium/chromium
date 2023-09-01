// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the wavy message container
 * in the ambient preview element.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ambient_zero_state_svg_element.html.js';

export class AmbientZeroStateSvgElement extends PolymerElement {
  static get is() {
    return 'ambient-zero-state-svg';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    AmbientZeroStateSvgElement.is, AmbientZeroStateSvgElement);
