// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Icons specific to personalization app.
 * This file is run in both trusted and untrusted code, and therefore
 * cannot import polymer and iron-iconset-svg itself. Any consumer should
 * import necessary dependencies before this file.
 *
 * NOTE: this file cannot be migrated to typescript because the import problem
 * above.
 *
 * These icons should have transparent fill color to adapt to its container's
 * light/dark theme.
 *
 * Following the demo here:
 * @see https://github.com/PolymerElements/iron-iconset-svg/blob/v3.0.1/demo/svg-sample-icons.js
 */

const template = document.createElement('template');
template.innerHTML = `{__html_template__}`;
document.head.appendChild(template.content);
