// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the wavy message container
 * in the ambient preview element.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './info_svg_element.html.js';

export class InfoSvgElement extends PolymerElement {
  static get is() {
    return 'info-svg';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(InfoSvgElement.is, InfoSvgElement);
