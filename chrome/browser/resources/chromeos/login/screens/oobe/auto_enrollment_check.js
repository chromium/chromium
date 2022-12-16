// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Auto-enrollment check screen implementation.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {afterNextRender, dom, flush, html, mixinBehaviors, Polymer, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 */
const AutoEnrollmentCheckElementBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

class AutoEnrollmentCheckElement extends AutoEnrollmentCheckElementBase {
  static get is() {
    return 'auto-enrollment-check-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('AutoEnrollmentCheckScreen');
  }
}

customElements.define(
    AutoEnrollmentCheckElement.is, AutoEnrollmentCheckElement);
