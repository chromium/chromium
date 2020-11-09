// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines TutorialCommon, a Polymer behavior to perform generic
 * tutorial-related functions.
 */

/**
 * @polymerBehavior
 * @suppress {undefinedVars|missingProperties}
 */
export const TutorialCommon = {
  /**
   * Returns the message with the given message id from the ChromeVox namespace.
   * @param {string} idOrValue The id of the string, or the hard-coded string
   *     value.
   * @param {Array<string>=} opt_subs Substitution strings.
   * @return {string} The localized message.
   */
  getMsg(idOrValue, opt_subs) {
    return Msgs.getMsg(idOrValue, opt_subs);
  }
};
