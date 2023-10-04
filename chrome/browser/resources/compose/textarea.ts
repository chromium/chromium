// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './textarea.html.js';

export interface ComposeTextareaElement {
  $: {
    editButtonContainer: HTMLElement,
    input: HTMLTextAreaElement,
    readonlyText: HTMLElement,
  };
}

export class ComposeTextareaElement extends PolymerElement {
  static get is() {
    return 'compose-textarea';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      allowExitingReadonlyMode: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      readonly: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      value: {
        type: String,
        notify: true,
      },
    };
  }

  allowExitingReadonlyMode: boolean;
  readonly: boolean;
  value: string;

  private shouldShowEditIcon_(): boolean {
    return this.allowExitingReadonlyMode && this.readonly;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compose-textarea': ComposeTextareaElement;
  }
}

customElements.define(ComposeTextareaElement.is, ComposeTextareaElement);
