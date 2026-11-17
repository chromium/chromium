// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SharePasswordGroupAvatarElement} from './share_password_group_avatar.js';

export function getHtml(this: SharePasswordGroupAvatarElement) {
  return html`<!--_html_template_start_-->
<div class="inner-container" ?hidden="${this.isMembersCountLessThan_(2)}">
  <!-- Left / Top-Left -->
  <img is="cr-auto-img" draggable="false" alt="" id="secondImg"
      auto-src="${this.getProfileImageUrl_(1)}">
  <!-- Bottom-Left -->
  <img is="cr-auto-img" draggable="false" alt="" id="fourthImg"
      auto-src="${this.getProfileImageUrl_(3)}"
      ?hidden="${this.isMembersCountLessThan_(4)}">
</div>
<div class="inner-container">
  <!-- Top-Right -->
  <img is="cr-auto-img" draggable="false" alt="" id="thirdImg"
      auto-src="${this.getProfileImageUrl_(2)}"
      ?hidden="${this.isMembersCountLessThan_(3)}">
  <!-- Full size / Right / Bottom-Right -->
  <img is="cr-auto-img" draggable="false" alt="" id="firstImg"
      auto-src="${this.getProfileImageUrl_(0)}"
      ?hidden="${!this.isMembersCountLessThan_(5)}">
  <!-- Bottom-Right -->
  <div id="more" ?hidden="${this.isMembersCountLessThan_(5)}">
    <span draggable="false">+${this.getMoreCount_()}</span>
  </div>
</div>
<!--_html_template_end_-->`;
}
