// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OnboardingBackgroundElement} from './onboarding_background.js';

export function getHtml(this: OnboardingBackgroundElement) {
  return html`<!--_html_template_start_-->
<div id="container">
  <div id="canvas">
    <div class="shape" id="logo" @click="${this.onLogoClick_}"></div>

    <!-- Lines surrounding logo. -->
    <div class="line-container" id="blue-line">
      <div class="line"><div class="line-fill"></div></div>
    </div>
    <div class="line-container" id="green-line">
      <div class="line"><div class="line-fill"></div></div>
    </div>
    <div class="line-container" id="red-line">
      <div class="line"><div class="line-fill"></div></div>
    </div>
    <div class="line-container" id="grey-line">
      <div class="line"><div class="line-fill"></div></div>
    </div>
    <div class="line-container" id="yellow-line">
      <div class="line"><div class="line-fill"></div></div>
    </div>

    <!-- Grey dotted lines surrounding logo. -->
    <div class="shape dotted-line" id="dotted-line-1"></div>
    <div class="shape dotted-line" id="dotted-line-2"></div>
    <div class="shape dotted-line" id="dotted-line-3"></div>
    <div class="shape dotted-line" id="dotted-line-4"></div>

    <!-- Connectagons. -->
    <div class="connectagon-container" id="yellow-connectagon">
      <div class="connectagon">
        <div class="circle"></div>
        <div class="square"></div>
        <div class="hexagon"></div>
      </div>
    </div>
    <div class="connectagon-container" id="blue-connectagon">
      <div class="connectagon">
        <div class="hexagon"></div>
        <div class="square"></div>
        <div class="hexagon"></div>
      </div>
    </div>
    <div class="connectagon-container" id="green-connectagon">
      <div class="connectagon">
        <div class="circle"></div>
        <div class="square"></div>
        <div class="circle"></div>
      </div>
    </div>

    <!-- Colored shapes. -->
    <div class="shape" id="green-triangle"></div>
    <div class="shape" id="square"></div>

    <!-- Grey shapes. -->
    <div class="shape" id="grey-triangle"></div>
    <div class="shape circle" id="grey-circle-1"></div>
    <div class="shape circle" id="grey-circle-2"></div>
    <div class="shape" id="grey-lozenge"></div>

    <!-- Password field image. -->
    <div class="shape" id="password-field"></div>
    <div class="shape" id="password-field-input"></div>

    <!-- Bookmarks image. -->
    <div class="shape" id="bookmarks-background"></div>
    <div class="shape" id="bookmarks-foreground"></div>

    <!-- Connected devices image. -->
    <div class="shape" id="devices"></div>
    <div class="shape" id="devices-check"></div>
    <div class="shape" id="devices-circle">
      <div id="devices-circle-image"></div>
    </div>

    <!-- Temp: Overlay of graphic -->
    <div id="overlay"></div>
  </div>
</div>

<cr-icon-button id="playPause" iron-icon="${this.getPlayPauseIcon_()}"
    @click="${this.onPlayPauseClick_}"
    aria-label="${this.getPlayPauseLabel_()}">
</cr-icon-button>
<!--_html_template_end_-->`;
}
