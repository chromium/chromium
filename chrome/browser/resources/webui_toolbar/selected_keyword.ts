// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icon_from_table.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './selected_keyword.css.js';
import {getHtml} from './selected_keyword.html.js';
import type {SelectedKeywordState} from './toolbar_ui_api_data_model.mojom-webui.js';

export class SelectedKeywordElement extends CrLitElement {
  static get is() {
    return 'selected-keyword';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedKeywordState: {type: Object},
    };
  }

  accessor selectedKeywordState: SelectedKeywordState = {
    shortName: '',
    fullName: '',
    icon: {handleId: 0n},
  };
}

declare global {
  interface HTMLElementTagNameMap {
    'selected-keyword': SelectedKeywordElement;
  }
}

customElements.define(SelectedKeywordElement.is, SelectedKeywordElement);
