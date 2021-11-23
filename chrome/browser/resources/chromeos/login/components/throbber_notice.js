// Copyright 2015 The Chromium Authors. All rights reserved.
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
}

customElements.define(ThrobberNotice.is, ThrobberNotice);