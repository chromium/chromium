// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const ThrobberNoticeBase = Polymer.mixinBehaviors([OobeI18nBehavior],
                                                  Polymer.Element);

class ThrobberNotice extends ThrobberNoticeBase {

  static get is() {
    return 'throbber-notice';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {textKey: String};
  }

  constructor() {
    super();
  }

  /**
   * Returns the a11y message to be shown on this throbber, if the textkey is set.
   * @param {string} locale
   * @returns
   */
  getAriaLabel(locale) {
    return (!this.textKey) ? '' : this.i18n(this.textKey);
  }
}

customElements.define(ThrobberNotice.is, ThrobberNotice);