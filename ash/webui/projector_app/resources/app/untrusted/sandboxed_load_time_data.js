// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Minimal version of load_time_data.js for chrome-untrusted://
 * origins. They are sandboxed, so cannot use chrome://resources ("unable to
 * load local resource") which load_time_data.js relies on through strings.js.
 * Since we don't want to maintain a "mirror" of all the module dependencies on
 * each chrome-untrusted:// origin. For simplicity, this version lacks all the
 * validation done by load_time_data.js, and just aims to provide a compatible
 * API.
 */

const impl = {
  /**
   * Needs to be separate from data since some tast tests expect a data_ field.
   * TODO(b/192977700): Remove this.
   * @type {!Object}
   */
  data_: {},
  /**
   * Used by strings.js to populate loadTimeData.
   * Note we don't provide a getter since the original load_time_data object
   * interface explicitly disallows reading the data object directly.
   * @param {!Object} value
   */
  set data(value) {
    impl.data_ = value;
  },
  getValue: (id) => impl.data_[id],
  getString: (id) => /** @type{string} */ (impl.data_[id]),
  getBoolean: (id) => /** @type{boolean} */ (impl.data_[id]),
  getInteger: (id) => /** @type{number} */ (impl.data_[id]),
  valueExists: (id) => impl.data_[id] !== undefined,
};
window['loadTimeData'] = impl;
