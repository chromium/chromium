// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './taskbox/ai_taskbox.js';
import './memory_banks/memory_banks.js';
import './tab_groups/tab_groups.js';
import './memory_bank_chat/memory_bank_chat.js';
import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export type ViewType =
    'ai-taskbox'|'memory-banks'|'tab-groups'|'memory-bank-chat';

const VALID_VIEWS: Set<ViewType> =
    new Set(['ai-taskbox', 'memory-banks', 'tab-groups']);
const STORAGE_KEY = 'context_hub_current_view';

// Page refresh should restore the current view.
function getInitialView(): ViewType {
  const hash = window.location.hash.slice(1) as ViewType;
  if (VALID_VIEWS.has(hash)) {
    return hash;
  }
  const stored = localStorage.getItem(STORAGE_KEY) as ViewType;
  if (stored && VALID_VIEWS.has(stored)) {
    return stored;
  }
  return 'ai-taskbox';
}

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

  protected accessor currentView_: ViewType = getInitialView();

  override connectedCallback() {
    super.connectedCallback();
    window.addEventListener('hashchange', this.onHashChange_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('hashchange', this.onHashChange_);
  }

  private onHashChange_ = () => {
    const hash = window.location.hash.slice(1) as ViewType;
    if (VALID_VIEWS.has(hash) && this.currentView_ !== hash) {
      this.currentView_ = hash;
    }
  };

  protected onSelectedChanged_(e: CustomEvent<{value: ViewType}>) {
    this.currentView_ = e.detail.value;
    window.location.hash = this.currentView_;
    localStorage.setItem(STORAGE_KEY, this.currentView_);
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
