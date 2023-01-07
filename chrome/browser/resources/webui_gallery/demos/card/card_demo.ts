// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_page_host_style.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './card_demo.html.js';

class CardDemoElement extends PolymerElement {
  static get is() {
    return 'card-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expanded_: Boolean,
    };
  }

  private expanded_: boolean = false;

  private onExternalLinkClick_() {
    window.open('https://chromium.org');
  }
}

customElements.define(CardDemoElement.is, CardDemoElement);
