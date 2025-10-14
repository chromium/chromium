// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox.js';
import '/strings.m.js';

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './full_app.css.js';
import {getHtml} from './full_app.html.js';

export class OmniboxFullAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-full-app';
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
    };
  }

  protected accessor omniboxPopupDebugEnabled_ =
      loadTimeData.getBoolean('omniboxPopupDebugEnabled');

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

  private onVisibilitychange_() {
    if (document.visibilityState !== 'visible') {
      return;
    }

    const searchbox = this.shadowRoot.querySelector('cr-searchbox');
    assert(searchbox);
    searchbox.focusInput();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-full-app': OmniboxFullAppElement;
  }
}

customElements.define(OmniboxFullAppElement.is, OmniboxFullAppElement);
