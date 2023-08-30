// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../demo.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_chip_demo.html.js';

class CrChipDemoElement extends PolymerElement {
  static get is() {
    return 'cr-chip-demo';
  }

  static get template() {
    return getTemplate();
  }
}

export const tagName = CrChipDemoElement.is;

customElements.define(CrChipDemoElement.is, CrChipDemoElement);
