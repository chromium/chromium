// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/dropdown/dropdown_option.js';
import './cra/cra-dropdown.js';
import './cra/cra-icon.js';

import {
  createRef,
  css,
  html,
  map,
  PropertyDeclarations,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';
import {LangPackInfo, LanguageCode} from '../core/soda/language_info.js';
import {
  assertEnumVariant,
  assertInstanceof,
} from '../core/utils/assert.js';

import {CraDropdown} from './cra/cra-dropdown.js';

/**
 * Dropdown menu that display available languages.
 */
export class LanguageDropdown extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;

      /* Avoid clicking outside make dropdown focused */
      width: fit-content;
    }
  `;

  static override properties: PropertyDeclarations = {
    languageList: {attribute: false},
    defaultLanguage: {attribute: false},
  };

  static override shadowRootOptions: ShadowRootInit = {
    ...ReactiveLitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  languageList: LangPackInfo[] = [];

  // Default language option of the dropdown. It's users' responsibility to make
  // sure `defaultLanguage` is included in `languageList`.
  defaultLanguage = LanguageCode.EN_US;

  private readonly dropdown = createRef<CraDropdown>();

  override async getUpdateComplete(): Promise<boolean> {
    await this.dropdown.value?.updateComplete;
    return super.getUpdateComplete();
  }

  private onChanged(ev: Event) {
    const dropdownValue = assertEnumVariant(
      LanguageCode,
      assertInstanceof(ev.target, CraDropdown).value,
    );
    this.dispatchEvent(
      new CustomEvent('dropdown-changed', {detail: dropdownValue}),
    );
  }

  private renderDropdownOptions(): RenderResult {
    return map(this.languageList, (langPack) => {
      return html`
        <cros-dropdown-option
          headline=${langPack.displayName}
          value=${langPack.languageCode}
          ?selected=${langPack.languageCode === this.defaultLanguage}
        >
        </cros-dropdown-option>
      `;
    });
  }

  override render(): RenderResult {
    // CrOS dropdown does not support showing default text when no item
    // selected. Select the hint string by-default as a workaround.
    return html`
      <cra-dropdown @change=${this.onChanged} ${ref(this.dropdown)}>
        <cra-icon name="language" slot="leading"></cra-icon>
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
