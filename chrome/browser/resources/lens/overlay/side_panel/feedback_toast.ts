// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '/strings.m.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './feedback_toast.css.js';
import {getHtml} from './feedback_toast.html.js';
import {SidePanelBrowserProxyImpl} from './side_panel_browser_proxy.js';
import type {SidePanelBrowserProxy} from './side_panel_browser_proxy.js';

export interface FeedbackToastElement {
  $: {
    closeFeedbackToastButton: CrButtonElement,
    feedbackToast: CrToastElement,
  };
}

export class FeedbackToastElement extends CrLitElement {
  static get is() {
    return 'feedback-toast';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      darkMode: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  // Whether to render the feedback toast in dark mode.
  protected accessor darkMode: boolean = loadTimeData.getBoolean('darkMode');

  private browserProxy: SidePanelBrowserProxy =
      SidePanelBrowserProxyImpl.getInstance();

  show() {
    if (this.$.feedbackToast.open) {
      // If toast already open, wait after hiding so that animation is
      // smoother.
      this.hide();
      setTimeout(() => {
        this.$.feedbackToast.show();
      }, 100);
      return;
    }
    this.$.feedbackToast.show();
  }

  hide() {
    this.$.feedbackToast.hide();
  }

  protected onSendFeedbackClick() {
    this.browserProxy.handler.requestSendFeedback();
    this.hide();
  }

  protected onHideFeedbackToastClick() {
    this.hide();
    this.dispatchEvent(new CustomEvent('feedback-toast-dismissed'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feedback-toast': FeedbackToastElement;
  }
}

customElements.define(FeedbackToastElement.is, FeedbackToastElement);
