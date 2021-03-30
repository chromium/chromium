// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines Localization, a Polymer behavior to help localize
 * tutorial content.
 */

/**
 * @polymerBehavior
 * @suppress {undefinedVars|missingProperties}
 */
export const Localization = {
  /**
   * Returns the message with the given message id from the ChromeVox namespace.
   * @param {string} id The id of the string.
   * @param {Array<string>=} opt_subs Substitution strings.
   * @return {string} The localized message.
   */
  getMsg(id, opt_subs) {
    return Msgs.getMsg(id, opt_subs);
  }
};
