// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './translate_button.html.js';

export interface TranslateButtonElement {
  $: {
    languagePicker: HTMLDivElement,
    translateButton: CrButtonElement,
  };
}

export class TranslateButtonElement extends PolymerElement {
  static get is() {
    return 'translate-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isTranslateModeEnabled: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  // Whether the translate mode on the lens overlay has been enabled.
  private isTranslateModeEnabled: boolean = false;

  private onTranslateButtonClick() {
    // Toggle translate mode on button click.
    this.isTranslateModeEnabled = !this.isTranslateModeEnabled;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'translate-button': TranslateButtonElement;
  }
}

customElements.define(TranslateButtonElement.is, TranslateButtonElement);
