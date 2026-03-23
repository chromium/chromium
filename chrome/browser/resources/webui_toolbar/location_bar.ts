// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './readonly_omnibox.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxViewState} from './browser_proxy.js';
import {getCss} from './location_bar.css.js';
import {getHtml} from './location_bar.html.js';

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
      omniboxViewState: {type: Object},
    };
  }

  protected accessor omniboxViewState: OmniboxViewState = {
    text: '',
    selection: null,
  };
}

declare global {
  interface HTMLElementTagNameMap {
    'location-bar': LocationBarElement;
  }
}

customElements.define(LocationBarElement.is, LocationBarElement);
