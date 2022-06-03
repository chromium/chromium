// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const ProgressListItemBase =
    Polymer.mixinBehaviors([OobeI18nBehavior], Polymer.Element);

/**
 * @polymer
 */
class ProgressListItem extends ProgressListItemBase {
  static get is() {
    return 'progress-list-item';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
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
    };
  }

  constructor() {
    super();
  }

  /** @private */
  hidePending_(active, completed) {
    return active || completed;
  }

  /** @private */
  hideCompleted_(active, completed) {
    return active || !completed;
  }

  /** @private */
  fallbackText(locale, key, fallbackKey) {
    if (key === null || key === '')
      return this.i18n(fallbackKey);
    return this.i18n(key);
  }
}

customElements.define(ProgressListItem.is, ProgressListItem);
