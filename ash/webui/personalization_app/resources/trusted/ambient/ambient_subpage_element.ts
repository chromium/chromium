// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ambient-subpage component displays the main content of
 * the ambient mode settings.
 */

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class AmbientSubpage extends PolymerElement {
  static get is() {
    return 'ambient-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // TODO: Toggle row related, initial values will be read by a provider.
      ambientModeEnabled_: {type: Boolean, value: false},
      description_: {
        type: String,
        value:
            'When your screen is idle, show photos, time, weather, and media info'
      },
    };
  }

  private ambientModeEnabled_: boolean;
  private description_: string;

  private onClickAmbientModeButton_(event: Event) {
    event.stopPropagation();
    this.ambientModeEnabled_ = !this.ambientModeEnabled_;
  }
}

customElements.define(AmbientSubpage.is, AmbientSubpage);
