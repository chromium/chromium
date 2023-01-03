// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying pausable lottie animation.
 */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lottie/cr_lottie.js';
import './oobe_icons.html.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from './behaviors/oobe_i18n_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeCrLottieBase = mixinBehaviors([OobeI18nBehavior], PolymerElement);

/**
 * @typedef {{
 *   animation:  CrLottieElement,
 * }}
 */
OobeCrLottieBase.$;

/** @polymer */
export class OobeCrLottie extends OobeCrLottieBase {
  static get is() {
    return 'oobe-cr-lottie';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      playing: {
        type: Boolean,
        observer: 'onPlayingChanged_',
        value: false,
      },

      animationUrl: {
        type: String,
        value: '',
      },
    };
  }

  ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
  }

  onClick_() {
    this.playing = !this.playing;
  }

  onPlayingChanged_() {
    if (this.$) {
      this.$.animation.setPlay(this.playing);
    }
  }

  getIcon_(playing) {
    if (playing) {
      return 'oobe-48:pause';
    }
    return 'oobe-48:play';
  }

  getAria_() {
    if (this.playing) {
      return this.i18n('pauseAnimationAriaLabel');
    }
    return this.i18n('playAnimationAriaLabel');
  }
}

customElements.define(OobeCrLottie.is, OobeCrLottie);
