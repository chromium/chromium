// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './readonly_omnibox.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './location_bar.css.js';
import {getHtml} from './location_bar.html.js';
import type {LocationBarState} from './toolbar_ui_api_data_model.mojom-webui.js';

export class LocationBarElement extends CrLitElement {
  static get is() {
    return 'location-bar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      locationBarState: {type: Object},
    };
  }

  protected accessor locationBarState: LocationBarState = {
    omniboxViewState: {
      textPieces: [],
      selection: null,
      textIsUrl: false,
    },
    contentSettingImageStates: [],
  };
}

declare global {
  interface HTMLElementTagNameMap {
    'location-bar': LocationBarElement;
  }
}

customElements.define(LocationBarElement.is, LocationBarElement);
