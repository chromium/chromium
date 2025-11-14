// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox.js';
import '/strings.m.js';

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './aim_app.css.js';
import {getHtml} from './aim_app.html.js';

export class OmniboxAimAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-aim-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');

  private isDebug_: boolean =
      new URLSearchParams(window.location.search).has('debug');
  private eventTracker_ = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    const composebox = this.shadowRoot.querySelector('cr-composebox');
    assert(composebox);
    composebox.focusInput();

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
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-aim-app': OmniboxAimAppElement;
  }
}

customElements.define(OmniboxAimAppElement.is, OmniboxAimAppElement);
