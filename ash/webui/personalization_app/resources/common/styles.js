// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common styles for polymer components in both trusted and
 * untrusted code. Polymer must be imported before this file. This file cannot
 * import Polymer itself because trusted and untrusted code access polymer at
 * different paths.
 *
 * NOTE: this file cannot be migrated to typescript because the import problem
 * above.
 */

const template = document.createElement('dom-module');
template.innerHTML = `{__html_template__}`;
template.register('common-style');
