// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrA11yAnnouncerDemoElement} from './cr_a11y_announcer_demo.js';

export function getHtml(this: CrA11yAnnouncerDemoElement) {
  return html`
<h1>cr-a11y-announcer</h1>

<div class="demos">
  <cr-checkbox ?checked="${this.forceShowAnnouncer_}"
      @checked-changed="${this.onForceShowAnnouncerChanged_}">
    Force show announcer
  </cr-checkbox>

  <cr-button @click="${this.onAnnounceTextClick_}">
    Announce text
  </cr-button>

  <cr-button @click="${this.onAnnounceMultipleTextsClick_}">
    Announce multiple texts
  </cr-button>

  <div id="announcerContainer"></div>
</div>`;
}
