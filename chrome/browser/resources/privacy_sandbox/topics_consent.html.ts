// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicsConsent} from './topics_consent.js';

export function getHtml(this: TopicsConsent) {
  return html`
    <div>Topics Consent Placeholder</div>
    <cr-button id="consentButton" @click="${this.onConsentButton_}">
      Consent Placeholder
    </cr-button>
  `;
}
