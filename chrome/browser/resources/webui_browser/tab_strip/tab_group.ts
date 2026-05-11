// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabGroupVisualData} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';

import {Color} from '../tab_group_types.mojom-webui.js';

import {getCss} from './tab_group.css.js';
import {getHtml} from './tab_group.html.js';

export class TabGroupElement extends CrLitElement {
  static get is() {
    return 'webui-browser-tab-group';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      collectionId: {type: String},
      groupData: {type: Object},
    };
  }

  accessor collectionId: string = '';
  accessor groupData: TabGroupVisualData = {
    title: '',
    color: Color.kGrey,
    isCollapsed: false,
  };
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-tab-group': TabGroupElement;
  }
}

customElements.define(TabGroupElement.is, TabGroupElement);
