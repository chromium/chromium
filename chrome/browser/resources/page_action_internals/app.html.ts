// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PageActionInternalsAppElement} from './app.js';
import {ActionButtonType, IconType} from './page_action_internals.mojom-webui.js';

export function getHtml(this: PageActionInternalsAppElement) {
  return html`<!--_html_template_start_-->
<div class="container">
  <h1>Page Action & Anchored Message Internals</h1>

  <p class="description">
    This page allows you to configure and trigger the page action framework
    through a dedicated for-debug action. Production page actions are not
    triggerable via this UI to prevent browser crashes caused by forcing
    real features into invalid states.
  </p>

  <div class="form-section">
    <h2>Configuration</h2>

    <div class="form-group">
      <label for="pageActionIcon">Page Action Icon (Omnibox):</label>
      <select
        id="pageActionIcon"
        class="md-select"
        .value="${this.pageActionIcon !== null ?
          String(this.pageActionIcon) :
          'none'}"
        @change="${this.onPageActionIconChange}"
      >
        <option value="none">None</option>
        <option value="${String(IconType.kInfo)}">Info (Blue)</option>
        <option value="${String(IconType.kOrangeAFavicon)}">
          Orange 'A' Favicon
        </option>
      </select>
    </div>

    <div class="form-group">
      <cr-checkbox
        ?checked="${this.showChip}"
        @checked-changed="${this.onShowChipCheckedChanged}"
      >
        Show Chip (Omnibox text label)
      </cr-checkbox>
    </div>

    ${this.showChip
      ? html`
          <div class="form-group">
            <label for="chipText">Chip Text:</label>
            <cr-input
              id="chipText"
              class="stroked"
              .value="${this.chipText}"
              @value-changed="${this.onChipTextValueChanged}"
            >
            </cr-input>
          </div>
        `
      : ''}
  </div>

  <div class="form-section">
    <h2>Anchored Message Configuration</h2>

    <div class="form-group">
      <label for="messageText">Message Text:</label>
      <cr-input
        id="messageText"
        class="stroked"
        .value="${this.messageText}"
        @value-changed="${this.onMessageTextValueChanged}"
      >
      </cr-input>
    </div>

    <div class="form-group">
      <label for="bubbleIcon">Bubble Icon (Left side):</label>
      <select
        id="bubbleIcon"
        class="md-select"
        .value="${this.bubbleIcon !== null ?
          String(this.bubbleIcon) :
          'none'}"
        @change="${this.onBubbleIconChange}"
      >
        <option value="none">None</option>
        <option value="${String(IconType.kInfo)}">Info (Blue)</option>
        <option value="${String(IconType.kOrangeAFavicon)}">
          Orange 'A' Favicon
        </option>
      </select>
    </div>

    <div class="form-group">
      <label for="actionIcon">Action Icon (Right side):</label>
      <select
        id="actionIcon"
        class="md-select"
        .value="${this.actionIcon !== null ?
          String(this.actionIcon) :
          'none'}"
        @change="${this.onActionIconChange}"
      >
        <option value="none">None</option>
        <option value="${String(ActionButtonType.kClose)}">
          Close Button
        </option>
        <option value="${String(ActionButtonType.kMenu)}">
          3-Dot Menu
        </option>
      </select>
    </div>

    <div class="form-group">
      <cr-checkbox
        ?checked="${this.hasDrawer}"
        @checked-changed="${this.onHasDrawerCheckedChanged}"
      >
        Include Expandable Drawer (Bottom section)
      </cr-checkbox>
    </div>

    ${this.hasDrawer
      ? html`
          <div class="drawer-config">
            <div class="form-group">
              <label for="drawerHeading">Drawer Heading:</label>
              <cr-input
                id="drawerHeading"
                class="stroked"
                .value="${this.drawerHeading}"
                @value-changed="${this.onDrawerHeadingValueChanged}"
              >
              </cr-input>
            </div>

            <div class="form-group">
              <label for="drawerIcon">Drawer Items Icon:</label>
              <select
                id="drawerIcon"
                class="md-select"
                .value="${this.drawerIcon !== null ?
                  String(this.drawerIcon) :
                  'none'}"
                @change="${this.onDrawerIconChange}"
              >
                <option value="none">None</option>
                <option value="${String(IconType.kInfo)}">
                  Info (Blue)
                </option>
                <option value="${String(IconType.kOrangeAFavicon)}">
                  Orange 'A' Favicon
                </option>
              </select>
            </div>

            <div class="form-group">
              <label>Drawer Items (Arbitrary number):</label>
              <div class="drawer-items-list">
                ${this.drawerItems.map(
                  (item, index) => html`
                    <div class="drawer-item-row">
                      <cr-input
                        class="stroked"
                        .value="${item}"
                        data-index="${index}"
                        @value-changed="${this.onDrawerItemValueChanged}"
                      >
                      </cr-input>
                      <cr-button
                        data-index="${index}"
                        @click="${this.onRemoveDrawerItemClick}"
                      >
                        Remove
                      </cr-button>
                    </div>
                  `,
                )}
              </div>
              <div>
                <cr-button @click="${this.onAddDrawerItemClick}">
                  + Add Item
                </cr-button>
              </div>
            </div>
          </div>
        `
      : ''}
  </div>

  <div class="actions-section">
    <cr-button class="action-button" @click="${this.onShowPageActionClick}">
      Show Page Action
    </cr-button>
    <cr-button
      class="action-button"
      @click="${this.onShowAnchoredMessageClick}"
    >
      Show Anchored Message
    </cr-button>
    <cr-button @click="${this.onHideClick}"> Hide/Dismiss </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
}
