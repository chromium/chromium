// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const parentAccessHandler =
    parentAccessUi.mojom.ParentAccessUIHandler.getRemote();

Polymer({
  is: 'parent-access-ui',

  _template: html`{__html_template__}`,

  /** @override */
  ready() {
    // TODO(danan): implement ready() fn.
  },
});
