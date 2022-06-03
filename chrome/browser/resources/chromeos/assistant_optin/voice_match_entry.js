// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'voice-match-entry',

  behaviors: [OobeI18nBehavior],

  properties: {
    active: {
      type: Boolean,
      value: false,
    },

    completed: {
      type: Boolean,
      value: false,
    },
  },
});
