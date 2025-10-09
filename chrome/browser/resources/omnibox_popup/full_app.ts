// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox.js';
import '/strings.m.js';

import {EventTracker} from '//resources/js/event_tracker.js';
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

  private isDebug_: boolean =
      new URLSearchParams(window.location.search).has('debug');
  private eventTracker_ = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

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
    'omnibox-full-app': OmniboxFullAppElement;
  }
}

customElements.define(OmniboxFullAppElement.is, OmniboxFullAppElement);
