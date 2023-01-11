// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import '//resources/cr_elements/icons.html.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_url_list_item_demo.html.js';

class CrUrlListItemDemoElement extends PolymerElement {
  static get is() {
    return 'cr-url-list-item-demo';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(CrUrlListItemDemoElement.is, CrUrlListItemDemoElement);
