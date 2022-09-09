// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for lacros data backward migration screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_loading_dialog.m.js';
import '../../components/oobe_icons.m.js';
import '../../components/oobe_slide.m.js';

import {assert} from '//resources/js/assert.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.m.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.m.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.m.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const LacrosDataBackwardMigrationScreenElementBase = mixinBehaviors(
    [
      OobeDialogHostBehavior,
      OobeI18nBehavior,
      LoginScreenBehavior,
      MultiStepBehavior,
    ],
    PolymerElement);

class LacrosDataBackwardMigrationScreen extends
    LacrosDataBackwardMigrationScreenElementBase {
  static get is() {
    return 'lacros-data-backward-migration-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      progressValue_: {
        type: Number,
        value: 0,
      },
    };
  }

  defaultUIStep() {
    return 'progress';
  }

  get UI_STEPS() {
    return {
      PROGRESS: 'progress',
    };
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('LacrosDataBackwardMigrationScreen');
  }
}

customElements.define(
    LacrosDataBackwardMigrationScreen.is, LacrosDataBackwardMigrationScreen);
