// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox.js';
import '/strings.m.js';

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './aim_app.html.js';

export class OmniboxAimAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-aim-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

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

    const composebox = this.shadowRoot.querySelector('ntp-composebox');
    assert(composebox);
    composebox.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-aim-app': OmniboxAimAppElement;
  }
}

customElements.define(OmniboxAimAppElement.is, OmniboxAimAppElement);
