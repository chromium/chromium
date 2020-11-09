// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'progress-list-item',

  behaviors: [OobeI18nBehavior],

  properties: {
    /* The ID of the localized string to be used as button text.
     */
    textKey: {
      type: String,
    },

    /* The ID of the localized string to be displayed when item is in
     * the 'active' state. If not specified, the textKey is used instead.
     */
    activeKey: {
      type: String,
      value: '',
    },

    /* The ID of the localized string to be displayed when item is in the
     * 'completed' state. If not specified, the textKey is used instead.
     */
    completedKey: {
      type: String,
      value: '',
    },

    /* Indicates if item is in "active" state. Has higher priority than
     * "completed" below.
     */
    active: {
      type: Boolean,
      value: false,
    },

    /* Indicates if item is in "completed" state. Has lower priority than
     * "active" state above.
     */
    completed: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  hidePending_(active, completed) {
    return active || completed;
  },

  /** @private */
  hideCompleted_(active, completed) {
    return active || !completed;
  },

  /** @private */
  fallbackText(locale, key, fallbackKey) {
    if (key === null || key === '')
      return this.i18n(fallbackKey);
    return this.i18n(key);
  },
});
