// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LoadingStateElement} from './loading_state.js';

export function getHtml(this: LoadingStateElement) {
  return html`
  <div id="loadingContainer">
    <cr-loading-gradient id="gradientContainer">
      <svg id="svg" height="511">
        <clippath id="clipPath"></clippath>
      </svg>
    </cr-loading-gradient>
  </div>
  <div id="lastColumnGradient"></div>`;
}
