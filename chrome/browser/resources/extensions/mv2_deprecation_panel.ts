// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './mv2_deprecation_panel.html.js';

export class ExtensionsMv2DeprecationPanelElement extends PolymerElement {
  static get is() {
    return 'extensions-mv2-deprecation-panel';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /*
       * Extensions to display in the panel.
       */
      extensions: Array,
    };
  }

  extensions: chrome.developerPrivate.ExtensionInfo[];
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-mv2-deprecation-panel': ExtensionsMv2DeprecationPanelElement;
  }
}

customElements.define(
    ExtensionsMv2DeprecationPanelElement.is,
    ExtensionsMv2DeprecationPanelElement);
