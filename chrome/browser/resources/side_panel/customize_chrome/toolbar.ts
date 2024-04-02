// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';
import './strings.m.js';

import type {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './toolbar.html.js';

export interface ToolbarElement {
  $: {
    heading: SpHeading,
  };
}

export class ToolbarElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-toolbar': ToolbarElement;
  }
}

customElements.define(ToolbarElement.is, ToolbarElement);
