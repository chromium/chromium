// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WallpaperSearchTileElement} from './wallpaper_search_tile.js';

export function getHtml(this: WallpaperSearchTileElement) {
  return html`<!--_html_template_start_-->
<svg id="light" width="154" height="154" viewBox="0 0 154 154" fill="none" xmlns="http://www.w3.org/2000/svg">
  <ellipse cx="43.5416" cy="22.329" rx="43.5416" ry="22.329" transform="matrix(-0.864094 -0.503331 -0.503331 0.864094 174.269 126.832)"
      fill="var(--color-sys-ai-illustration-shape-surface1)"></ellipse>
  <path d="M114.168 143.505C93.3886 131.401 81.5756 112.95 87.7827 102.294L69.2385 134.13C63.0314 144.786 74.8444 163.236 95.6236 175.34C116.403 187.444 138.28 188.618 144.487 177.962L163.031 146.126C156.824 156.782 134.947 155.608 114.168 143.505Z"
      fill="url(#ovoidBottomLight)"></path>
  <path d="M146.758 79.9175L127.402 31.8674L166.472 -12.3016L185.828 35.7484L146.758 79.9175Z"
      fill="url(#cubeRightLight)"></path>
  <path d="M77.3485 18.5215L127.402 31.8672L166.472 -12.3018L116.419 -25.6476L77.3485 18.5215Z"
      fill="var(--color-sys-ai-illustration-shape-surface1)"></path>
  <path d="M127.401 31.8674L146.757 79.9174L96.7033 66.5716L77.348 18.5216L127.401 31.8674Z"
      fill="var(--color-sys-ai-illustration-shape-surface2)"></path>
  <path fill-rule="evenodd" clip-rule="evenodd" d="M-7.90683 65.2558L-28.0873 86.3182L-28.0764 86.3286C-31.9566 90.6697 -34.2646 96.4345 -34.1303 102.715C-33.8747 114.673 -24.8527 124.385 -13.3324 125.845C-11.3813 137.293 -1.29234 145.891 10.6661 145.636C16.9465 145.501 22.6073 142.949 26.7787 138.887L26.7895 138.897L57.2299 107.126L53.4939 103.547C53.5752 102.657 53.6074 101.753 53.5879 100.839C53.3323 88.8803 44.3098 79.168 32.7892 77.7083C30.8384 66.2605 20.7492 57.6614 8.7905 57.9173C7.87602 57.9368 6.97468 58.0077 6.08915 58.127L2.35314 54.5474L-7.90683 65.2558Z"
      fill="url(#flowerBottomLight)"></path>
  <path d="M15.408 47.6671C3.96105 49.619 -4.63736 59.7079 -4.38179 71.666C-4.12621 83.6244 4.89584 93.3365 16.4161 94.7965C18.3672 106.244 28.4562 114.843 40.4147 114.587C52.3727 114.331 62.0849 105.309 63.5456 93.7891C74.993 91.8378 83.592 81.7486 83.3364 69.7901C83.0808 57.8314 74.0584 48.1192 62.5377 46.6595C60.5869 35.2117 50.4978 26.6126 38.539 26.8684C26.5806 27.1243 16.8682 36.1467 15.408 47.6671Z"
      fill="var(--color-sys-ai-illustration-shape-surface2)"></path>
  <defs>
    <linearGradient id="ovoidBottomLight" x1="134.826" y1="132.513" x2="95.0544"
        y2="159.17" gradientUnits="userSpaceOnUse">
      <stop stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-start)"></stop>
      <stop offset="0.980117" stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-end)"></stop>
    </linearGradient>
    <linearGradient id="cubeRightLight" x1="136.553" y1="66.3807" x2="128.393"
        y2="21.3795" gradientUnits="userSpaceOnUse">
      <stop stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-start)"></stop>
      <stop offset="0.980117" stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-end)"></stop>
    </linearGradient>
    <linearGradient id="flowerBottomLight" x1="26.0926" y1="69.1576" x2="18.1937" y2="124.196" gradientUnits="userSpaceOnUse">
      <stop stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-start)"></stop>
      <stop offset="0.980117" stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-end)"></stop>
    </linearGradient>
  </defs>
</svg>

<svg id="dark" width="154" height="154" viewBox="0 0 154 154" fill="none" xmlns="http://www.w3.org/2000/svg">
  <ellipse cx="43.5416" cy="22.329" rx="43.5416" ry="22.329" transform="matrix(-0.864094 -0.503331 -0.503331 0.864094 174.269 126.832)"
      fill="var(--color-sys-ai-illustration-shape-surface1)"></ellipse>
  <path d="M114.168 143.505C93.3886 131.401 81.5756 112.95 87.7827 102.294L69.2385 134.13C63.0314 144.786 74.8444 163.236 95.6236 175.34C116.403 187.444 138.28 188.618 144.487 177.962L163.031 146.126C156.824 156.782 134.947 155.608 114.168 143.505Z"
      fill="url(#ovoidBottomDark)"></path>
  <path d="M146.758 79.9175L127.402 31.8674L166.472 -12.3016L185.828 35.7484L146.758 79.9175Z"
      fill="url(#cubeRightDark)"></path>
  <path d="M77.3485 18.5215L127.402 31.8672L166.472 -12.3018L116.419 -25.6476L77.3485 18.5215Z"
      fill="var(--color-sys-ai-illustration-shape-surface1)"></path>
  <path d="M127.401 31.8674L146.757 79.9174L96.7033 66.5716L77.348 18.5216L127.401 31.8674Z"
      fill="var(--color-sys-ai-illustration-shape-surface2)"></path>
  <path fill-rule="evenodd" clip-rule="evenodd" d="M-7.90622 65.2558L-28.0866 86.3182L-28.0758 86.3286C-31.956 90.6697 -34.2639 96.4345 -34.1297 102.715C-33.8741 114.673 -24.8521 124.385 -13.3318 125.845C-11.3807 137.293 -1.29173 145.891 10.6667 145.636C16.9471 145.501 22.6079 142.949 26.7793 138.887L26.7901 138.897L57.2305 107.126L53.4945 103.547C53.5758 102.657 53.608 101.753 53.5885 100.839C53.3329 88.8803 44.3104 79.168 32.7898 77.7083C30.839 66.2605 20.7498 57.6614 8.79111 57.9173C7.87663 57.9368 6.97529 58.0077 6.08976 58.127L2.35375 54.5474L-7.90622 65.2558Z"
      fill="url(#flowerBottomDark)"></path>
  <path d="M15.4087 47.6671C3.96178 49.619 -4.63663 59.7079 -4.38106 71.666C-4.12548 83.6244 4.89657 93.3365 16.4169 94.7965C18.368 106.244 28.4569 114.843 40.4154 114.587C52.3735 114.331 62.0856 105.309 63.5463 93.7891C74.9938 91.8378 83.5927 81.7486 83.3371 69.7901C83.0815 57.8314 74.0591 48.1192 62.5384 46.6595C60.5876 35.2117 50.4985 26.6126 38.5398 26.8684C26.5813 27.1243 16.869 36.1467 15.4087 47.6671Z"
      fill="var(--color-sys-ai-illustration-shape-surface2)"></path>
  <defs>
    <linearGradient id="ovoidBottomDark" x1="134.826" y1="132.513" x2="95.0544" y2="159.17" gradientUnits="userSpaceOnUse">
      <stop stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-start)"></stop>
      <stop offset="0.980117" stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-end)"></stop>
    </linearGradient>
    <linearGradient id="cubeRightDark" x1="136.553" y1="66.3807" x2="128.393" y2="21.3795" gradientUnits="userSpaceOnUse">
      <stop stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-start)"></stop>
      <stop offset="0.980117" stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-end)"></stop>
    </linearGradient>
    <linearGradient id="flowerBottomDark" x1="26.0932" y1="69.1576" x2="18.1943" y2="124.196" gradientUnits="userSpaceOnUse">
      <stop stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-start)"></stop>
      <stop offset="0.980117" stop-color="var(--color-sys-ai-illustration-shape-surface-gradient-end)"></stop>
    </linearGradient>
  </defs>
</svg>
<!--_html_template_end_-->`;
}
