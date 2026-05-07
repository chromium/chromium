// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SignInCelebrationAppElement} from './app.js';

export function getHtml(this: SignInCelebrationAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="headerContainer">
  <cr-lottie id="avatarAnimation"
      animation-url="${this.getAnimationUrl_()}"
      single-loop
      ?autoplay="${!this.disableAnimations_}">
  </cr-lottie>
  <img id="avatar" src="${this.userInfo_.avatarUrl}" alt="">
</div>
<h1 id="title" class="title">${this.userInfo_.title}</h1>
<!--_html_template_end_-->`;
  // clang-format on
}
