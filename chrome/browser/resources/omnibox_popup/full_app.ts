// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './omnibox_popup_searchbox.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './full_app.html.js';

export class OmniboxFullAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-full-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      webuiShadowEnabled_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  // When true, this page paints the popup's background, rounded corners and
  // drop shadow instead of the Views frame.
  protected accessor webuiShadowEnabled_: boolean =
      loadTimeData.getBoolean('omniboxFullWebUIShadowEnabled');

  private eventTracker_ = new EventTracker();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    // Force an initial refresh to avoid the race condition where the profile
    // theme loads after the page, but before the listener is ready.
    ColorChangeUpdater.forDocument().refreshColorsCss();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    performance.mark('OmniboxFullAppElement::firstUpdated');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-full-app': OmniboxFullAppElement;
  }
}

customElements.define(OmniboxFullAppElement.is, OmniboxFullAppElement);
