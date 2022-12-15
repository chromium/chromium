// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '../oobe_cr_lottie.js';
import '../buttons/oobe_text_button.js';
import '../common_styles/oobe_common_styles.m.js';
import '../common_styles/oobe_dialog_host_styles.m.js';
import './oobe_adaptive_dialog.js';
import './oobe_content_dialog.js';

import {assert} from '//resources/ash/common/assert.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeDialogHostBehavior} from '../behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../behaviors/oobe_i18n_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeLoadingDialogBase =
    mixinBehaviors([OobeI18nBehavior, OobeDialogHostBehavior], PolymerElement);


/** @polymer */
export class OobeLoadingDialog extends OobeLoadingDialogBase {
  static get template() {
    return html`{__html_template__}`;
  }

  static get is() {
    return 'oobe-loading-dialog';
  }

  static get properties() {
    return {
      titleKey: {
        type: String,
      },

      titleLabelKey: {
        type: String,
      },

      subtitleKey: {
        type: String,
        value: '',
      },

      /*
       * If true loading step can be canceled by pressing a cancel button.
       */
      canCancel: {
        type: Boolean,
        value: false,
      },
    };
  }

  onBeforeShow() {
    this.$.spinner.playing = true;
  }

  onBeforeHide() {
    this.$.spinner.playing = false;
  }

  // Returns either the passed 'title-label-key', or uses the 'title-key'.
  getAriaLabel(locale, titleLabelKey, titleKey) {
    assert(this.titleLabelKey || this.titleKey,
           'OOBE Loading dialog requires a title or a label for a11y!');
    return (titleLabelKey) ? this.i18n(titleLabelKey) : this.i18n(titleKey);
  }

  cancel() {
    assert(this.canCancel);
    this.dispatchEvent(
        new CustomEvent('cancel-loading', {bubbles: true, composed: true}));
  }
}

customElements.define(OobeLoadingDialog.is, OobeLoadingDialog);