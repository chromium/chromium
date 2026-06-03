// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './taskbox/ai_taskbox.js';
import './memory_banks/memory_banks.js';
import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export type ViewType = 'ai-taskbox'|'memory-banks';

export class ContextHubAppElement extends CrLitElement {
  static get is() {
    return 'context-hub-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentView_: {type: String},
    };
  }

  protected accessor currentView_: ViewType = 'ai-taskbox';

  protected onSelectedChanged_(e: CustomEvent<{value: ViewType}>) {
    this.currentView_ = e.detail.value;
  }

  protected onSelectorClick_(e: MouseEvent) {
    e.preventDefault();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'context-hub-app': ContextHubAppElement;
  }
}

customElements.define(ContextHubAppElement.is, ContextHubAppElement);
