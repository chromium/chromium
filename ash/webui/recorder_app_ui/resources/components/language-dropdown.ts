// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-dropdown.js';

import {
  css,
  html,
  map,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {LangPackInfo, LanguageCode} from '../core/soda/language_info.js';
import {assertInstanceof, checkEnumVariant} from '../core/utils/assert.js';

import {CraDropdown} from './cra/cra-dropdown.js';

/**
 * Dropdown menu that display available languages.
 */
export class LanguageDropdown extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }
  `;

  static override properties: PropertyDeclarations = {
    languageList: {attribute: false},
  };

  languageList: LangPackInfo[] = [];

  value: LanguageCode|null = null;

  private onChanged(ev: Event) {
    this.value = checkEnumVariant(
      LanguageCode,
      assertInstanceof(ev.target, CraDropdown).value,
    );
  }

  private renderDropdownOptions(): RenderResult {
    return map(this.languageList, (langPack) => {
      return html`
        <cros-dropdown-option
          headline=${langPack.displayName}
          value=${langPack.languageCode}
        >
        </cros-dropdown-option>
      `;
    });
  }

  override render(): RenderResult {
    // CrOS dropdown does not support showing default text when no item
    // selected. Use a disabled hint text as default option as a workaround.
    // TODO: b/377629564 - Modify CrOS dropdown to support default text for
    // correct a11y behavior.
    // TODO(hsuanling): Add leading icon after UI spec is done.
    return html`
    <cra-dropdown
      @change=${this.onChanged}
    >
      <cros-dropdown-option
        headline=${i18n.languageDropdownHintOption}
        selected
        disabled
      >
      </cros-dropdown-option>
      ${this.renderDropdownOptions()}
    </cra-dropdown>
  `;
  }
}

window.customElements.define('language-dropdown', LanguageDropdown);

declare global {
  interface HTMLElementTagNameMap {
    'language-dropdown': LanguageDropdown;
  }
}
