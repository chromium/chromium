// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/checkbox/checkbox.js';
import './cra/cra-icon-button.js';
import './cra/cra-icon.js';

import {
  classMap,
  css,
  CSSResultGroup,
  html,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';

export class ExpandableCard extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    #container {
      border: 1px solid var(--cros-sys-separator);
      border-radius: 12px;
    }

    #header {
      align-items: center;
      display: flex;
      flex-flow: row;
      gap: 8px;
      padding: 4px;

      &.disabled {
        color: var(--cros-sys-disabled);
      }
    }

    slot[name="header"]::slotted(*) {
      flex: 1;
    }

    #content {
      align-items: stretch;
      display: flex;
      flex-flow: column;
      padding: 8px 16px 16px;
    }
  `;

  static override properties: PropertyDeclarations = {
    expanded: {type: Boolean},
    disabled: {type: Boolean},
  };

  expanded = false;

  disabled = false;

  private onToggleExpand() {
    this.dispatchEvent(new CustomEvent('toggle-expand'));
  }

  private get shouldExpand(): boolean {
    return this.expanded && !this.disabled;
  }

  private renderContent(): RenderResult {
    if (!this.shouldExpand) {
      return nothing;
    }
    return html`
      <div id="content">
        <slot name="content"></slot>
      </div>
    `;
  }

  override render(): RenderResult {
    const classes = {
      disabled: this.disabled,
    };

    return html`<div id="container">
      <div id="header" class=${classMap(classes)}>
        <cros-checkbox
          ?checked=${this.shouldExpand}
          @change=${this.onToggleExpand}
          ?disabled=${this.disabled}
        >
        </cros-checkbox>
        <slot name="header"></slot>
        <cra-icon-button
          @click=${this.onToggleExpand}
          buttonstyle="floating"
          ?disabled=${this.disabled}
        >
          <cra-icon
            name=${this.shouldExpand ? 'chevron_up' : 'chevron_down'}
            slot="icon"
          ></cra-icon>
        </cra-icon-button>
      </div>
      ${this.renderContent()}
    </div>`;
  }
}

window.customElements.define('expandable-card', ExpandableCard);

declare global {
  interface HTMLElementTagNameMap {
    'expandable-card': ExpandableCard;
  }
}
