// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';

Polymer({
  is: 'app-management-supported-links-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @type {!App} */
    app: Object,
  },
});
