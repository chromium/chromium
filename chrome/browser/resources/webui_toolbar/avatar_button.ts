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
import {HelpBubbleAnchorMixin} from './toolbar_button.js';
import type {AvatarControlState} from './toolbar_ui_api_data_model.mojom-webui.js';

const AvatarButtonElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class AvatarButtonElement extends AvatarButtonElementBase {
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
      ...super.properties,
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

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(
        this.state?.accessibilityDescription || '');
  }

  protected onClick_(_e: Event) {
    BrowserProxyImpl.getInstance().toolbarUIHandler.showAvatarMenu();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'avatar-button': AvatarButtonElement;
  }
}

customElements.define(AvatarButtonElement.is, AvatarButtonElement);
