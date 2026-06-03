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
            <cr-menu-selector id="selector" selectable="a" selected-attribute="selected"
                attr-for-selected="data-route"
                selected="${this.currentView_}"
                @selected-changed="${this.onSelectedChanged_}"
                @click="${this.onSelectorClick_}">
                <a role="menuitem" href="#" data-route="ai-taskbox" class="cr-nav-menu-item">
                    <cr-icon icon="cr:extension"></cr-icon>
                    AI Taskbox
                </a>
                <a role="menuitem" href="#" data-route="memory-banks" class="cr-nav-menu-item">
                    <cr-icon icon="cr:history"></cr-icon>
                    Memory banks
                </a>
            </cr-menu-selector>
        </div>
    </aside>

    <!-- CONTENT AREA -->
    <div class="content-area">
        ${
      this.currentView_ === 'ai-taskbox' ? html`
          <ai-taskbox></ai-taskbox>
        ` :
                                           ''}
        ${
      this.currentView_ === 'memory-banks' ? html`
          <memory-banks></memory-banks>
        ` :
                                             ''}
    </div>
  `;
}
