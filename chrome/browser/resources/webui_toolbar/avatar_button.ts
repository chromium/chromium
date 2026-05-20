// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './avatar_button.css.js';
import {getHtml} from './avatar_button.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {AvatarControlState} from './toolbar_ui_api_data_model.mojom-webui.js';

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

  protected accessor state: AvatarControlState = {
    iconUrl: '',
    text: '',
    tooltip: '',
    accessibilityName: '',
    accessibilityDescription: '',
  };

  protected onClick_(_: Event) {
    // TODO(behamilton): Log an error if this fails.
    BrowserProxyImpl.getInstance().toolbarUIHandler.showAvatarMenu();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'avatar-button': AvatarButtonElement;
  }
}

customElements.define(AvatarButtonElement.is, AvatarButtonElement);
