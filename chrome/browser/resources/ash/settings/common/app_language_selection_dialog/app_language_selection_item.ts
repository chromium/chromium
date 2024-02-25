// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'app-language-selection-item' is part of app language picker
 * dialog to display language option in a single row.
 */
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import '../../settings_shared.css.js';

import {Locale} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_language_selection_item.html.js';

export class AppLanguageSelectionItemElement extends PolymerElement {
  static get is() {
    return 'app-language-selection-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      index: Number,
      item: Object,
      selected: {
        type: Boolean,
        value: false,
      },
    };
  }

  index: number;
  item: Locale;
  selected: boolean;

  private getDisplayText_(): string {
    let name = this.item.displayName;
    if (name === '') {
      // If displayName couldn't be translated, we'll display the localeTag
      // instead.
      return this.item.localeTag;
    }
    if (this.item.nativeDisplayName !== '' &&
        this.item.nativeDisplayName !== name) {
      name += ' - ' + this.item.nativeDisplayName;
    }
    return name;
  }

  private getAriaSelected_(): string {
    return this.selected ? 'true' : 'false';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppLanguageSelectionItemElement.is]: AppLanguageSelectionItemElement;
  }
}

customElements.define(
    AppLanguageSelectionItemElement.is, AppLanguageSelectionItemElement);
