// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import './common_styles/oobe_common_styles.m.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from './behaviors/oobe_i18n_behavior.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const ThrobberNoticeBase = mixinBehaviors([OobeI18nBehavior], PolymerElement);

/** @polymer */
class ThrobberNotice extends ThrobberNoticeBase {

  static get is() {
    return 'throbber-notice';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {textKey: String};
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
