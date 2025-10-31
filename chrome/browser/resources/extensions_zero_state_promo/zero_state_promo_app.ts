// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/js/action_link.js';
import '/strings.m.js';

import type {CustomHelpBubbleHandlerInterface} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble.mojom-webui.js';
import {CustomHelpBubbleUserAction} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble.mojom-webui.js';
import {CustomHelpBubbleProxyImpl} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {WebStoreLinkClicked} from './zero_state_promo.mojom-webui.js';
import {getCss} from './zero_state_promo_app.css.js';
import {getHtml} from './zero_state_promo_app.html.js';
import type {ZeroStatePromoBrowserProxy} from './zero_state_promo_browser_proxy.js';
import {ZeroStatePromoBrowserProxyImpl} from './zero_state_promo_browser_proxy.js';

export class ZeroStatePromoAppElement extends CrLitElement {
  static get is() {
    return 'extensions-zero-state-promo-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showChipsUi_: {type: Boolean},
    };
  }

  private apiProxy_: ZeroStatePromoBrowserProxy =
      ZeroStatePromoBrowserProxyImpl.getInstance();
  private customHelpBubbleHandler_: CustomHelpBubbleHandlerInterface =
      CustomHelpBubbleProxyImpl.getInstance().getHandler();

  protected accessor showChipsUi_: boolean =
      loadTimeData.getBoolean('showChipsUi');

  protected onChromeWebStoreButtonClick_() {
    this.apiProxy_.launchWebStoreLink(WebStoreLinkClicked.kDiscoverExtension);
    this.customHelpBubbleHandler_.notifyUserAction(
        CustomHelpBubbleUserAction.kAction);
  }

  protected onCouponsButtonClick_() {
    this.apiProxy_.launchWebStoreLink(WebStoreLinkClicked.kCoupon);
    this.customHelpBubbleHandler_.notifyUserAction(
        CustomHelpBubbleUserAction.kAction);
  }

  protected onWritingButtonClick_() {
    this.apiProxy_.launchWebStoreLink(WebStoreLinkClicked.kWriting);
    this.customHelpBubbleHandler_.notifyUserAction(
        CustomHelpBubbleUserAction.kAction);
  }

  protected onProductivityButtonClick_() {
    this.apiProxy_.launchWebStoreLink(WebStoreLinkClicked.kProductivity);
    this.customHelpBubbleHandler_.notifyUserAction(
        CustomHelpBubbleUserAction.kAction);
  }

  protected onAiButtonClick_() {
    this.apiProxy_.launchWebStoreLink(WebStoreLinkClicked.kAi);
    this.customHelpBubbleHandler_.notifyUserAction(
        CustomHelpBubbleUserAction.kAction);
  }

  protected onDismissButtonClick_() {
    this.customHelpBubbleHandler_.notifyUserAction(
        CustomHelpBubbleUserAction.kDismiss);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-zero-state-promo-app': ZeroStatePromoAppElement;
  }
}

customElements.define(ZeroStatePromoAppElement.is, ZeroStatePromoAppElement);
