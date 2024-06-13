// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProgressIndicatorDemoElement} from './progress_indicator_demo.js';

export function getHtml(this: ProgressIndicatorDemoElement) {
  return html`
<h1>cr-progress</h1>
<div class="demos">
  <cr-progress indeterminate></cr-progress>
</div>

<h1>spinner, 28px</h1>
<div class="demos">
  <div class="spinner"></div>
</div>

<h1>Loading gradients</h1>
<div class="demos">
  <h2>Avatar icon clipPath</h2>
  <cr-loading-gradient>
    <svg width="24" height="24">
      <clipPath>
        <path
          d="M12 12c2.21 0 4-1.79 4-4s-1.79-4-4-4-4 1.79-4 4 1.79 4 4 4zm0
              2c-2.67 0-8 1.34-8 4v2h16v-2c0-2.66-5.33-4-8-4z">
        </path>
      </clipPath>
    </svg>
  </cr-loading-gradient>

  <h2>Rectangular clipPath</h2>
  <cr-loading-gradient>
    <svg width="100%" height="5">
      <clipPath>
        <rect x="0" y="0" width="100%" height="5" rx="5"></rect>
      </clipPath>
    </svg>
  </cr-loading-gradient>

  <h2>Multiple regions clipPath with external border</h2>
  <div id="gradientWithBorder">
    <cr-loading-gradient>
      <svg width="100%" height="64">
        <clipPath>
          <rect x="0" y="0" width="100%" height="16" rx="8"></rect>
          <rect x="0" y="24" width="100%" height="16" rx="8"></rect>
          <rect x="0" y="48" width="100%" height="16" rx="8"></rect>
        </clipPath>
      </svg>
    </cr-loading-gradient>
  </div>

  <h2>Custom colors</h2>
  <cr-loading-gradient id="customColors">
    <svg width="200" height="200">
      <clipPath>
        <circle cx="100" cy="100" r="100"></circle>
      </clipPath>
    </svg>
  </cr-loading-gradient>
</div>`;
}
