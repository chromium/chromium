// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_segmented_button/cr_segmented_button.js';
import '//resources/cr_elements/cr_segmented_button/cr_segmented_button_option.js';
import '../demo.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_radio_demo.html.js';

class CrRadioDemoElement extends PolymerElement {
  static get is() {
    return 'cr-radio-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedRadioOption_: String,
      selectedSegmentedButtonOption_: String,
    };
  }

  private selectedRadioOption_: string;
  private selectedSegmentedButtonOption_: string;
}

export const tagName = CrRadioDemoElement.is;

customElements.define(CrRadioDemoElement.is, CrRadioDemoElement);
