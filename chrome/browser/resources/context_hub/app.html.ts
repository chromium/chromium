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
          <a role="menuitem"
              href="#topics"
              data-route="topics"
              class="cr-nav-menu-item">
            <cr-icon icon="context-hub:topic"></cr-icon>
            Topics
          </a>
          <a role="menuitem"
              href="#jumpstart"
              data-route="jumpstart"
              class="cr-nav-menu-item">
            <cr-icon icon="cr:search"></cr-icon>
            JumpStart
          </a>
        </cr-menu-selector>
      </div>
    </aside>

    <!-- CONTENT AREA -->
    <div class="content-area">
      ${(() => {
    switch (this.currentView_) {
      case 'launchpad':
        return html`<ai-taskbox></ai-taskbox>`;
      case 'memory-banks':
        return html`<memory-banks></memory-banks>`;
      case 'memory-bank-chat':
        return html`<memory-bank-chat></memory-bank-chat>`;
      case 'tab-groups':
        return html`<tab-groups></tab-groups>`;
      case 'topics':
        return html`<topics-view></topics-view>`;
      case 'jumpstart':
        return html`<smart-search></smart-search>`;
      default:
        return '';
    }
  })()}
    </div>
  `;
}
