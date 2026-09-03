// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextHubAppElement} from './app.js';

export function getHtml(this: ContextHubAppElement) {
  return html`
    <!-- SIDEBAR -->
    <aside>
      <div>
        <!-- Logo Section -->
        <div class="logo-section">
          <cr-icon icon="cr:chrome-product"></cr-icon>
          <span id="sidebar-logo-text">Context Hub</span>
        </div>

        <!-- Navigation Links -->
        <cr-menu-selector id="selector"
            selectable="a:not(.disabled)"
            selected-attribute="selected"
            attr-for-selected="data-route"
            selected="${this.currentView_}"
            @selected-changed="${this.onSelectedChanged_}"
            @click="${this.onSelectorClick_}">
          <a role="menuitem"
              href="#launchpad"
              data-route="launchpad"
              class="cr-nav-menu-item">
            <cr-icon icon="cr:chrome-extension-filled"></cr-icon>
            LaunchPad
          </a>
          <a role="menuitem"
              href="#memory-banks"
              data-route="memory-banks"
              class="cr-nav-menu-item">
            <cr-icon icon="cr:history"></cr-icon>
            Memory banks
          </a>
          <a role="menuitem"
              aria-disabled="true"
              tabindex="-1"
              class="cr-nav-menu-item disabled">
            <cr-icon icon="cr:draft-filled"></cr-icon>
            Memory Bank Chat
          </a>
          <a role="menuitem"
              href="#tab-groups"
              data-route="tab-groups"
              class="cr-nav-menu-item">
            <cr-icon icon="cr:domain"></cr-icon>
            Tab groups
          </a>
        </cr-menu-selector>
      </div>
    </aside>

    <!-- CONTENT AREA -->
    <div class="content-area">
      ${this.currentView_ === 'launchpad' ? html`
        <ai-taskbox></ai-taskbox>
      ` : ''}
      ${this.currentView_ === 'memory-banks' ? html`
        <memory-banks></memory-banks>
      ` : ''}
      ${this.currentView_ === 'memory-bank-chat' ? html`
        <memory-bank-chat></memory-bank-chat>
      ` : ''}
      ${this.currentView_ === 'tab-groups' ? html`
        <tab-groups></tab-groups>
      ` : ''}
    </div>
  `;
}
