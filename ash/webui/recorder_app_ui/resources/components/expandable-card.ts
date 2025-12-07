// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/accordion/accordion.js';
import 'chrome://resources/cros_components/accordion/accordion_item.js';
import 'chrome://resources/cros_components/checkbox/checkbox.js';
import './cra/cra-icon-button.js';
import './cra/cra-icon.js';

import {
  Checkbox as CrosCheckbox,
} from 'chrome://resources/cros_components/checkbox/checkbox.js';
import {
  css,
  CSSResultGroup,
  html,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';
import {assertInstanceof} from '../core/utils/assert.js';

export class ExpandableCard extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    cros-accordion {
      --cros-card-background-color: none;

      &::part(card) {
        width: initial;
      }
    }

    cros-checkbox {
      margin: -8px -14px;
    }

    cros-accordion-item {
      &::part(content) {
        padding: 0 14px 18px;
      }

      &::part(row) {
        line-height: 1;
      }
    }

    slot[name="content"]::slotted(*) {
      width: 100%;
    }
  `;

  static override properties: PropertyDeclarations = {
    expanded: {type: Boolean},
    disabled: {type: Boolean},
  };

  expanded = false;

  disabled = false;

  private onExpanded() {
    this.dispatchEvent(new CustomEvent('expandable-card-expanded'));
  }

  private onCollapsed() {
    this.dispatchEvent(new CustomEvent('expandable-card-collapsed'));
  }

  private onCheckboxClick(ev: Event) {
    const target = assertInstanceof(ev.target, CrosCheckbox);
    if (target.checked) {
      this.onExpanded();
    } else {
      this.onCollapsed();
    }
  }

  private get shouldExpand(): boolean {
    return this.expanded && !this.disabled;
  }

  override render(): RenderResult {
    // The whole header of cros-accordion can be focused, so tabindex=-1 is set
    // on the checkbox to make it unfocusable.
    return html`<cros-accordion variant="compact">
      <cros-accordion-item
        ?disabled=${this.disabled}
        ?expanded=${this.shouldExpand}
        @cros-accordion-item-expanded=${this.onExpanded}
        @cros-accordion-item-collapsed=${this.onCollapsed}
      >
        <cros-checkbox
          slot="leading"
          ?checked=${this.shouldExpand}
          ?disabled=${this.disabled}
          @change=${this.onCheckboxClick}
          tabindex="-1"
          aria-hidden="true"
        >
        </cros-checkbox>
        <slot name="header" slot="title"></slot>
        <slot name="content"></slot>
      </cros-accordion-item>
    </cros-accordion>`;
  }
}

window.customElements.define('expandable-card', ExpandableCard);

declare global {
  interface HTMLElementTagNameMap {
    'expandable-card': ExpandableCard;
  }
}
