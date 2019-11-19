// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview First user log in Marketing Opt-In screen implementation.
 */

login.createScreen('MarketingOptInScreen', 'marketing-opt-in', function() {
  return {
    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('marketing-opt-in-impl');
    },
  };
});
