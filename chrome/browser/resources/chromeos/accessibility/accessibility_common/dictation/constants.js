// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines constants for Dictation.
 */

/**
 * Defines a type for data used to update the Dictation bubble UI.
 * @typedef {{
 *    icon: !chrome.accessibilityPrivate.DictationBubbleIconType,
 *    text: (string|undefined),
 *    hints: (!Array<string>|undefined),
 * }}
 */
export let BubbleProperties;
