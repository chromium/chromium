// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/checkbox/checkbox.js';
import 'chrome://resources/cros_components/dropdown/dropdown_option.js';
import 'chrome://resources/mwc/@material/web/focus/md-focus-ring.js';
import './cra/cra-dropdown.js';
import './expandable-card.js';

import {
  Checkbox as CrosCheckbox,
} from 'chrome://resources/cros_components/checkbox/checkbox.js';
import {
  classMap,
  css,
  CSSResultGroup,
  html,
  map,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';
import {assertInstanceof} from '../core/utils/assert.js';

import {CraDropdown} from './cra/cra-dropdown.js';

interface DropdownOption {
  headline: string;
  value: string;
}

export class ExportDialogSection extends ReactiveLitElement {
  static override shadowRootOptions: ShadowRootInit = {
    ...ReactiveLitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  static override styles: CSSResultGroup = css`
    :host {
      display: contents;
    }

    slot[name="header"] {
      color: var(--cros-sys-on_surface);
      font: var(--cros-headline-1-font);
    }

    cros-dropdown-option cra-icon {
      color: var(--cros-sys-primary);
    }

    #container {
      border-radius: 12px;
      cursor: pointer;
      outline: 1px solid var(--cros-sys-separator);
      position: relative;

      &.disabled {
        opacity: var(--cros-disabled-opacity);
        pointer-events: none;
      }

      & > md-focus-ring {
        --md-focus-ring-active-width: 2px;
        --md-focus-ring-color: var(--cros-sys-focus_ring);
        --md-focus-ring-duration: 0s;
        --md-focus-ring-shape: 12px;
        --md-focus-ring-width: 2px;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    checked: {type: Boolean},
    disabled: {type: Boolean},
    options: {attribute: false},
    value: {type: String},
  };

  checked = false;

  disabled = false;

  options: DropdownOption[] = [];

  value: string|null = null;

  private renderDropdownOptions(
    options: DropdownOption[],
    selected: string,
  ): RenderResult {
    return map(options, ({headline, value}) => {
      const icon = value === selected ?
        html`<cra-icon name="checked" slot="end"></cra-icon>` :
        nothing;
      return html`
        <cros-dropdown-option .headline=${headline} .value=${value}>
          ${icon}
        </cros-dropdown-option>
      `;
    });
  }

  private onCheckChange(checked: boolean) {
    this.dispatchEvent(new CustomEvent('check-changed', {detail: checked}));
  }

  private onValueChange(ev: Event) {
    const value = assertInstanceof(ev.target, CraDropdown).value;
    this.dispatchEvent(new CustomEvent('value-changed', {detail: value}));
  }

  private renderExpandableCard(): RenderResult {
    if (this.value === null) {
      return nothing;
    }
    // TODO: b/344784478 - Show estimate file size.
    const options = this.renderDropdownOptions(this.options, this.value);

    const onExpanded = () => {
      this.onCheckChange(true);
    };

    const onCollapsed = () => {
      this.onCheckChange(false);
    };

    return html`
      <expandable-card
        ?expanded=${this.checked}
        ?disabled=${this.disabled}
        @expandable-card-expanded=${onExpanded}
        @expandable-card-collapsed=${onCollapsed}
      >
        <slot slot="header" name="header"></slot>
        <cra-dropdown
          .value=${this.value}
          slot="content"
          @change=${this.onValueChange}
        >
          ${options}
        </cra-dropdown>
      </expandable-card>
    `;
  }

  private onRowClick() {
    this.onCheckChange(!this.checked);
  }

  private onRowKeyDown(e: KeyboardEvent) {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.onRowClick();
    }
  }

  /*
   * Renders the header without expandable part.
   *
   * Note that due to a11y difference, we can't just customize cros-accordion,
   * and need to implement the same style separately.
   *
   * In this case the .value is not used and .onValueChange is never called,
   * since there's no multiple options for value.
   */
  private renderRow(): RenderResult {
    const onCheckboxChange = (ev: Event) => {
      const checkbox = assertInstanceof(ev.target, CrosCheckbox);
      this.onCheckChange(checkbox.checked);
    };

    const classes = {
      disabled: this.disabled,
    };

    return html`<div
      id="container"
      class=${classMap(classes)}
      tabindex=${this.disabled ? -1 : 0}
      @click=${this.onRowClick}
      @keydown=${this.onRowKeyDown}
      role="checkbox"
      aria-checked=${this.checked}
      aria-disabled=${this.disabled}
    >
      <cros-checkbox
        slot="leading"
        ?checked=${this.checked}
        ?disabled=${this.disabled}
        @change=${onCheckboxChange}
        tabindex="-1"
        aria-hidden="true"
      >
      </cros-checkbox>
      <slot slot="header" name="header" id="header"></slot>
      <md-focus-ring inward></md-focus-ring>
    </div>`;
  }

  override render(): RenderResult {
    if (this.options.length <= 1) {
      return this.renderRow();
    } else {
      return this.renderExpandableCard();
    }
  }
}

window.customElements.define('export-dialog-section', ExportDialogSection);

declare global {
  interface HTMLElementTagNameMap {
    'export-dialog-section': ExportDialogSection;
  }
}
