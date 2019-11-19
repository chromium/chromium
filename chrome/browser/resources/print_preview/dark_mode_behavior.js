// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from 'chrome://resources/js/assert.m.js';

const prefersDark = window.matchMedia('(prefers-color-scheme: dark)');

/** @polymerBehavior */
export const DarkModeBehavior = {
  properties: {
    /** Whether or not the OS is in dark mode. */
    inDarkMode: {
      type: Boolean,
      value: prefersDark.matches,
    },
  },

  /** @override */
  attached() {
    this.boundOnChange_ = this.boundOnChange_ || this.onChange_.bind(this);
    prefersDark.addListener(this.boundOnChange_);
  },

  /** @override */
  detached() {
    prefersDark.removeListener(assert(this.boundOnChange_));
  },

  /** @private */
  onChange_() {
    this.inDarkMode = prefersDark.matches;
  },
};

DarkModeBehavior.inDarkMode = () => prefersDark.matches;
