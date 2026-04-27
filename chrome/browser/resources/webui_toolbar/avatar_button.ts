// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './avatar_button.css.js';
import {getHtml} from './avatar_button.html.js';

export interface AvatarButtonState {
  isVisible: boolean;
  accessibilityDescription: string;
  accessibilityName: string;
  isButtonActionDisabled: boolean;
}

export class AvatarButtonElement extends CrLitElement {
  static get is() {
    return 'avatar-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Object},
    };
  }

  protected accessor state: AvatarButtonState|null = null;

  protected getIcon(): string {
    return 'cr:person';
  }

  protected onClick_(_e: Event) {
    // TODO: Connect to browser controls handler.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'avatar-button': AvatarButtonElement;
  }
}

customElements.define(AvatarButtonElement.is, AvatarButtonElement);
