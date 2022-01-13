// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common styles for polymer components in both trusted and
 * untrusted code.
 */

// Force tsc to consider this file a module.
export {};

const template = document.createElement('dom-module');
template.innerHTML = `{__html_template__}`;
template.register('common-style');
