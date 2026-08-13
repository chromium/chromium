// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Time, TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

export interface PatternSourceData {
  primaryPattern: string;
  secondaryPattern: string;
  sourceElement: HTMLElement;
  valueElement: HTMLElement;
  incognitoElement: HTMLElement;
  lastModified: Time;
  lastUsed: Time;
  lastVisited: Time;
  expiration: Time;
  sessionModelElement: HTMLElement;
  lifetime: TimeDelta;
}

export function getHtml(data: PatternSourceData) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="content-setting">
  <div class="patterns">
    <div class="pattern">
      <span class="pattern-type">Primary</span>
      <span class="block id-primary-pattern" title="${data.primaryPattern}">${data.primaryPattern}</span>
    </div>

    <div class="pattern">
      <span class="block pattern-type">Secondary</span>
      <span class="block id-secondary-pattern" title="${data.secondaryPattern}">${data.secondaryPattern}</span>
    </div>

    <div class="pattern">
      <span class="block pattern-type">Source</span>
      <span class="block id-source">
        ${data.sourceElement}
      </span>
    </div>
  </div>

  <div class="metadata">
    <div>
      <span class="label">Value:</span>
      <span class="value id-value">
        ${data.valueElement}
      </span>
    </div>
    <div>
      <span class="label">Incognito:</span>
      <span class="value id-incognito">
        ${data.incognitoElement}
      </span>
    </div>
    <div>
      <span class="label">Last Modified:</span>
      <span class="value id-last-modified">
        <mojo-timestamp ts="${data.lastModified.internalValue}"></mojo-timestamp>
      </span>
    </div>
    <div>
      <span class="label">Last Used:</span>
      <span class="value id-last-used">
        <mojo-timestamp ts="${data.lastUsed.internalValue}"></mojo-timestamp>
      </span>
    </div>
    <div>
      <span class="label">Last Visited:</span>
      <span class="value id-last-visited">
        <mojo-timestamp ts="${data.lastVisited.internalValue}"></mojo-timestamp>
      </span>
    </div>
    <div>
      <span class="label">Expiration:</span>
      <span class="value id-expiration">
        <mojo-timestamp ts="${data.expiration.internalValue}"></mojo-timestamp>
      </span>
    </div>
    <div>
      <span class="label">Session Model:</span>
      <span class="value id-session-model">
        ${data.sessionModelElement}
      </span>
    </div>
    <div>
      <span class="label">Lifetime:</span>
      <span class="value id-lifetime">
        <mojo-timedelta duration="${data.lifetime.microseconds}"></mojo-timedelta>
      </span>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
