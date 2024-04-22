// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';

import type {SpHeadingElement} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CustomizeToolbarHandlerInterface} from '../customize_toolbar.mojom-webui.js';

import {CustomizeToolbarApiProxy} from './customize_toolbar_api_proxy.js';
import {getTemplate} from './toolbar.html.js';

export interface ToolbarElement {
  $: {
    heading: SpHeadingElement,
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

  private handler_: CustomizeToolbarHandlerInterface;

  constructor() {
    super();
    this.handler_ = CustomizeToolbarApiProxy.getInstance().handler;
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
