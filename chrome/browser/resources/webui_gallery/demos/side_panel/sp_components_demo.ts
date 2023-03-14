// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//webui-gallery/shared/sp_filter_chip.js';
import '//webui-gallery/shared/sp_list_item_badge.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sp_components_demo.html.js';

class SpComponentsDemo extends PolymerElement {
  static get is() {
    return 'sp-components-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }
}

customElements.define(SpComponentsDemo.is, SpComponentsDemo);
