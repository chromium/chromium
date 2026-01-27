// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ScrollViewDemoElement} from './scroll_view_demo.js';

export function getHtml(this: ScrollViewDemoElement) {
  // clang-format off
  return html`
<h1>Scroll view with shadows indicating scroll</h1>

<h1>cr-scrollable</h1>
<div id="cr-scrollable-demos" class="demos">
  <div class="cr-scrollable">
    <div class="label">A normal scrollable element with no indicators.</div>
    <div class="block"></div>
  </div>
  <div class="cr-scrollable">
    <div class="cr-scrollable-top"></div>
    <div class="label">With borders indicating element is scrollable.</div>
    <div class="block"></div>
    <div class="cr-scrollable-bottom"></div>
  </div>
  <div class="cr-scrollable">
    <div class="cr-scrollable-top force-on"></div>
    <div class="label">With borders always visible.</div>
    <div class="cr-scrollable-bottom force-on"></div>
  </div>
  <div class="cr-scrollable">
    <div class="cr-scrollable-top-shadow"></div>
    <div class="label">With a top shadow indicating element is scrollable.</div>
    <div class="block"></div>
  </div>
  <div class="cr-scrollable">
    <div class="cr-scrollable-top-shadow force-on"></div>
    <div class="label">With a top shadow always visible.</div>
  </div>
</div>`;
  // clang-format on
}
