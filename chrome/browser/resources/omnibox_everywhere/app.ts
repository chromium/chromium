// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './omnibox.js';
import './composebox.js';
import '/strings.m.js';

import type {ComposeboxState} from '//resources/cr_components/composebox/common.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class OmniboxEverywhereAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-everywhere-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      omniboxPopupDebugEnabled_: {
        type: Boolean,
        reflect: true,
      },
      isComposeboxMode_: {type: Boolean},
      searchboxLayoutMode_: {type: String},
      caretAnimationsEnabled_: {type: Boolean},
      disableComposeboxAnimation_: {type: Boolean},
      disableVoiceSearchAnimation_: {type: Boolean},
      usePecApi_: {type: Boolean},
      isOblongShape_: {type: Boolean},
      contextManagementInComposeboxEnabled_: {type: Boolean},
      composeboxState_: {type: Object},
    };
  }

  protected accessor omniboxPopupDebugEnabled_ =
      loadTimeData.getBoolean('omniboxPopupDebugEnabled');
  protected accessor isComposeboxMode_: boolean = false;
  protected accessor searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor caretAnimationsEnabled_: boolean =
      loadTimeData.getBoolean('caretAnimationEnabled');
  protected accessor disableComposeboxAnimation_: boolean =
      loadTimeData.getBoolean('composeboxAnimationDisabled');
  protected accessor disableVoiceSearchAnimation_: boolean =
      !loadTimeData.getBoolean('voiceSearchCoherenceComposeboxesEnabled');
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');
  protected accessor contextManagementInComposeboxEnabled_: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');
  protected accessor composeboxState_: ComposeboxState|null = null;

  private isDebug_: boolean =
      new URLSearchParams(window.location.search).has('debug');
  private eventTracker_ = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document.documentElement, 'visibilitychange',
        this.onVisibilitychange_.bind(this));
    this.onVisibilitychange_();
    if (!this.isDebug_) {
      this.eventTracker_.add(
          document.documentElement, 'contextmenu', (e: Event) => {
            e.preventDefault();
          });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  protected async onOpenComposebox_(e: CustomEvent<ComposeboxState>) {
    this.composeboxState_ = e.detail;
    this.isComposeboxMode_ = true;
    await this.updateComplete;
    const composebox =
        this.shadowRoot.querySelector('omnibox-everywhere-composebox');
    if (composebox) {
      composebox.focusInput();
      composebox.playGlowAnimation();
    }
  }

  protected onCloseComposebox_() {
    this.isComposeboxMode_ = false;
  }

  protected onComposeboxSubmit_() {
    this.isComposeboxMode_ = false;
  }

  private async onVisibilitychange_() {
    if (document.visibilityState !== 'visible') {
      return;
    }

    await this.updateComplete;
    const searchbox =
        this.shadowRoot.querySelector('omnibox-everywhere-omnibox');
    if (searchbox) {
      searchbox.focusInput();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-app': OmniboxEverywhereAppElement;
  }
}

customElements.define(
    OmniboxEverywhereAppElement.is, OmniboxEverywhereAppElement);
