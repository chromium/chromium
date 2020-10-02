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
    // TODO(akihiroota): Remove this check after adding all strings to
    // chromevox_strings.grdp.
    // If we get a string that doesn't include a '_', then it's a hard-coded
    // string value. Return it, since a message id doesn't exist for it yet.
    if (!idOrValue.includes('_')) {
      return idOrValue;
    }
    return Msgs.getMsg(idOrValue, opt_subs);
  }
};
