// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar_chip_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import '/shared/icon_from_table.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AvatarControlState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {AvatarToolbarButtonState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {getCss} from './avatar_button.css.js';
import {getHtml} from './avatar_button.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {HelpBubbleAnchorMixin} from './toolbar_button.js';

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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('hasHelpBubble')) {
      BrowserProxyImpl.getInstance()
          .toolbarUIHandler.setAvatarButtonIphPromoShowing(this.hasHelpBubble);
    }
  }

  protected accessor state: AvatarControlState = {
    state: AvatarToolbarButtonState.kNormal,
    icon: {handleId: 0n},
    text: '',
    tooltip: '',
    accessibilityName: '',
    accessibilityDescription: '',
    enabled: true,
    hasAiRing: false,
  };

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(this.state?.tooltip || '');
  }

  protected shouldPaintBorder(): boolean {
    return !!this.state.text &&
        this.state.state === AvatarToolbarButtonState.kGuestSession;
  }

  protected getButtonClass_(): string {
    if (!this.state.text) {
      return '';
    }
    switch (this.state.state) {
      case AvatarToolbarButtonState.kSyncError:
        return 'highlight-sync-error';
      case AvatarToolbarButtonState.kGuestSession:
        return 'highlight-guest';
      case AvatarToolbarButtonState.kIncognitoProfile:
        return 'highlight-incognito';
      default:
        return 'highlight-default';
    }
  }

  protected onClick_(_: Event) {
    // TODO(behamilton): Log an error if this fails.
    BrowserProxyImpl.getInstance().toolbarUIHandler.showAvatarMenu();
  }

  protected onMouseenter_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.setAvatarButtonHovered(
        true);
  }

  protected onMouseleave_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.setAvatarButtonHovered(
        false);
  }

  protected onFocusin_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.setAvatarButtonFocused(
        true);
  }

  protected onFocusout_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.setAvatarButtonFocused(
        false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'avatar-button': AvatarButtonElement;
  }
}

customElements.define(AvatarButtonElement.is, AvatarButtonElement);
