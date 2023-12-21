// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  css,
} from 'chrome://resources/mwc/lit/index.js';

// Using :where selector here so the default style always have 0 specificity and
// can be overridden by each component.
export const DEFAULT_STYLE = css`
  /* https://web.dev/articles/custom-elements-best-practices#set-a-:host-display-style-e.g.-block,-inline-block,-flex-unless-you-prefer-the-default-of-inline. */
  :host {
    display: block;
  }

  :host([hidden]) {
    display: none;
  }

  :where(
    button,
    input[type=radio],
    input[type=checkbox],
    label
  ) {
    background-color: transparent;
    border-radius: 4px;
    border: none;
    margin: 0;
    padding: 0;
  }

  :where(
    input[type=radio],
    input[type=checkbox]
  ){
    -webkit-appearance: none;
  }

  :where(:focus-visible) {
    outline-offset: 3px;
    outline: 2px solid var(--cros-sys-focus_ring);
  }
`;
