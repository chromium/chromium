// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutoTabGroupsNotStartedImageElement} from './auto_tab_groups_not_started_image.js';

export function getHtml(this: AutoTabGroupsNotStartedImageElement) {
  return html`<!--_html_template_start_-->
<svg xmlns="http://www.w3.org/2000/svg" width="280" height="103" fill="none">
  <g clip-path="url(#a)">
    <rect width="280" height="103" fill="url(#b)" rx="8"></rect>
    <path class="window-frame-stroke window-frame-fill" stroke-width="2"
        d="M240 104h1V44a5 5 0 0 0-5-5H44a5 5 0 0 0-5 5v60h1z"></path>
    <path class="window-frame-fill"
        d="M40 44a4 4 0 0 1 4-4h192a4 4 0 0 1 4 4v12H40z"></path>
    <path fill="url(#d)"
        d="M40 56a4 4 0 0 1 4-4h192a4 4 0 0 1 4 4v47H40z"></path>
    <path class="tab-content-fill"
        d="M70 45a3 3 0 0 1 3-3h29a3 3 0 0 1 3 3v7H70z"></path>
    <path class="tab-content-fill" fill-rule="evenodd"
        d="M67 52a3 3 0 0 0 3-3v3zm41 0a3 3 0 0 1-3-3v3z"
        clip-rule="evenodd"></path>
    <rect class="tab-group-1-fill" width="13" height="6" x="53" y="42" rx="2">
    </rect>
    <path class="tab-group-1-stroke" stroke-linecap="round" stroke-width="2"
        d="M54 51h13a2 2 0 0 0 2-2v-4a4 4 0 0 1
           4-4h29a4 4 0 0 1 4 4v4a2 2 0 0 0 2 2h31">
    </path>
    <rect class="tab-group-2-fill" width="13" height="6" x="144" y="42" rx="2">
    </rect>
    <path class="tab-group-2-stroke" stroke-linecap="round" stroke-width="2"
        d="M145 51h80"></path>
    <rect class="active-tab-text-color"
        width="31" height="3" x="72" y="44" rx="1.5"></rect>
    <rect class="inactive-tab-text-color"
        width="30" height="3" x="109" y="44" rx="1.5"></rect>
    <rect class="inactive-tab-text-color"
        width="1" height="7" x="141" y="42" rx=".5"></rect>
    <rect class="inactive-tab-text-color"
        width="30" height="3" x="159" y="44" rx="1.5"></rect>
    <rect class="inactive-tab-text-color"
        width="1" height="7" x="191" y="42" rx=".5"></rect>
    <rect class="inactive-tab-text-color"
        width="30" height="3" x="194" y="44" rx="1.5"></rect>
    <rect class="inactive-tab-text-color"
        width="1" height="7" x="226" y="42" rx=".5"></rect>
  </g>
  <defs>
    <linearGradient id="b" x1="9.074" x2="72.077" y1="0" y2="177.007"
        gradientUnits="userSpaceOnUse">
      <stop class="image-background-color-1"></stop>
      <stop offset="1" class="image-background-color-2"></stop>
    </linearGradient>
    <linearGradient id="d" x1="140" x2="140" y1="80" y2="103"
        gradientUnits="userSpaceOnUse">
      <stop class="tab-content-stop-color-start"></stop>
      <stop offset="1" class="tab-content-stop-color-end"></stop>
    </linearGradient>
    <clipPath id="a">
      <rect width="280" height="103" fill="#fff" rx="8"></rect>
    </clipPath>
  </defs>
</svg>
<!--_html_template_end_-->`;
}
