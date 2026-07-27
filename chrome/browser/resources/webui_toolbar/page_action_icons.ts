// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_action_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageActionState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {PageActionId} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import type {PageActionIconElement} from './page_action_icon.js';
import {getCss} from './page_action_icons.css.js';
import {getHtml} from './page_action_icons.html.js';

export class PageActionIconsElement extends CrLitElement {
  static get is() {
    return 'page-action-icons';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageActionStates: {type: Array},
    };
  }

  aiModePageAction(): PageActionIconElement|null {
    const candidates = this.shadowRoot.querySelectorAll<PageActionIconElement>(
        'page-action-icon');
    for (const candidate of candidates) {
      if (candidate.state.pageActionId === PageActionId.kActionAiMode) {
        return candidate;
      }
    }
    return null;
  }

  accessor pageActionStates: PageActionState[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'page-action-icons': PageActionIconsElement;
  }
}

customElements.define(PageActionIconsElement.is, PageActionIconsElement);
