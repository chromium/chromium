// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview styles for polymer components in untrusted code.
 */

import 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';

const template = document.createElement('dom-module');
template.innerHTML = `{__html_template__}`;
template.register('untrusted-style');
