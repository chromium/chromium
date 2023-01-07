// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @fileoverview Polymer element for displaying pausable lottie animation.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeCrLottieBase =
    Polymer.mixinBehaviors([OobeI18nBehavior], Polymer.Element);

/**
 * @typedef {{
 *   animation:  CrLottieElement,
 * }}
 */
OobeCrLottieBase.$;

/* #export */ class OobeCrLottie extends OobeCrLottieBase {
  static get is() {
    return 'oobe-cr-lottie';
  }

  /* #html_template_placeholder */
  static get properties() {
    return {
      playing: {
        type: Boolean,
        observer: 'onPlayingChanged_',
      },
      animationUrl: String,
    };
  }

  constructor() {
    super();
    this.playing = false;
    this.animationUrl = '';
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
