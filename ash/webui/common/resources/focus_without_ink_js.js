// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This file is deprecated and retained only for legacy JS code that is
// still using closure compiler for type checking. New code should use
// focus_without_ink.ts.

// clang-format off
import {assert} from 'chrome://resources/ash/common/assert.js';
// clang-format on


let hideInk = false;

document.addEventListener('pointerdown', function() {
  hideInk = true;
}, true);

document.addEventListener('keydown', function() {
  hideInk = false;
}, true);

/**
 * Attempts to track whether focus outlines should be shown, and if they
 * shouldn't, removes the "ink" (ripple) from a control while focusing it.
 * This is helpful when a user is clicking/touching, because it's not super
 * helpful to show focus ripples in that case. This is Polymer-specific.
 * @param {!Element} toFocus
 */
export const focusWithoutInk = function(toFocus) {
  // |toFocus| does not have a 'noink' property, so it's unclear whether the
  // element has "ink" and/or whether it can be suppressed. Just focus().
  if (!('noink' in toFocus) || !hideInk) {
    toFocus.focus();
    return;
  }

  // Make sure the element is in the document we're listening to events on.
  assert(document === toFocus.ownerDocument);
  const {noink} = toFocus;
  toFocus.noink = true;
  toFocus.focus();
  toFocus.noink = noink;
};
