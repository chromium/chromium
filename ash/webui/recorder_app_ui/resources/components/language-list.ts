// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './language-list-item.js';

import {
  html,
  map,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {LangPackInfo, LanguageCode} from '../core/soda/language_info.js';

/**
 * A list of language options.
 */
export class LanguageList extends ReactiveLitElement {
  static override properties: PropertyDeclarations = {
    selectedLanguage: {attribute: false},
  };

  selectedLanguage: LanguageCode|null = null;

  private readonly platformHandler = usePlatformHandler();

  private renderLanguageRow(langPack: LangPackInfo): RenderResult {
    const {languageCode} = langPack;
    const sodaState = this.platformHandler.getSodaState(languageCode).value;

    return html`
      <language-list-item
        .langPackInfo=${langPack}
        .sodaState=${sodaState}
        ?selected=${languageCode === this.selectedLanguage}
      >
      </language-list-item>
    `;
  }

  override render(): RenderResult {
    const list = this.platformHandler.getLangPackList();
    return map(
      list,
      (langPack) => this.renderLanguageRow(langPack),
    );
  }
}

window.customElements.define('language-list', LanguageList);

declare global {
  interface HTMLElementTagNameMap {
    'language-list': LanguageList;
  }
}
