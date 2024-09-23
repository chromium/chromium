// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import '//resources/cr_elements/icons_lit.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_url_list_item_demo.css.js';
import {getHtml} from './cr_url_list_item_demo.html.js';

export class CrUrlListItemDemoElement extends CrLitElement {
  static get is() {
    return 'cr-url-list-item-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

export const tagName = CrUrlListItemDemoElement.is;

customElements.define(CrUrlListItemDemoElement.is, CrUrlListItemDemoElement);
