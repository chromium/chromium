// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_shortcuts.html.js';


export interface CustomizeChromePanelShortcutsElement {}

export class CustomizeChromePanelShortcutsElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-panel-shortcuts';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  override ready() {
    super.ready();
  }

  override connectedCallback() {
    super.connectedCallback();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-panel-shortcuts': CustomizeChromePanelShortcutsElement;
  }
}

customElements.define(
    CustomizeChromePanelShortcutsElement.is,
    CustomizeChromePanelShortcutsElement);
