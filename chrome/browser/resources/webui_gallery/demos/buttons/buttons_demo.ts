// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../demo.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './buttons_demo.html.js';

class ButtonsDemoElement extends PolymerElement {
  static get is() {
    return 'buttons-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expanded_: Boolean,
    };
  }

  private exapanded_: boolean;
}

export const tagName = ButtonsDemoElement.is;

customElements.define(ButtonsDemoElement.is, ButtonsDemoElement);
