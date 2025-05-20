// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicsConsentNotice} from './topics_consent_notice.js';

export function getHtml(this: TopicsConsentNotice) {
  return html`
    <div>Topics Consent Notice Placeholder</div>
    <cr-button id="acceptButton" @click="${this.onOptIn}">
      Accept Placeholder
    </cr-button>
  `;
}
