// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying pausable lottie animation.
 */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cros_components/lottie_renderer/lottie-renderer.js';
import './oobe_icons.html.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {traceOobeLottieExecution} from '../../oobe_trace.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from './behaviors/oobe_i18n_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeCrLottieBase = mixinBehaviors([OobeI18nBehavior], PolymerElement);

/**
 * @typedef {{
 *   animation: LottieRenderer,
 *   container: HTMLElement,
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
        observer: 'onUrlChanged_',
        value: '',
      },

      hidePlayPauseIcon: {
        type: Boolean,
        value: false,
      },

      preload: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether or not the illustration should render using a dynamic palette.
       * nuke this property when all animation migrated.
       */
      dynamic: {
        type: Boolean,
        value: true,
      },
    };
  }

  constructor() {
    super();
    this.animationPlayer = null;
  }

  ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
    // Preload the player so that the first frame is shown.
    if (this.preload) {
      this.createPlayer(/*autoplay=*/false);
    }
  }

  onClick_() {
    if (this.hidePlayPauseIcon) {
      return;
    }
    this.playing = !this.playing;
  }

  /**
   *
   * @param {?boolean} autoplay
   * @suppress {missingProperties}
   */
  createPlayer(autoplay = true) {
    this.animationPlayer = document.createElement('cros-lottie-renderer');
    this.animationPlayer.id = 'animation';
    this.animationPlayer.setAttribute('asset-url', this.animationUrl);
    this.animationPlayer.setAttribute('dynamic', this.dynamic);
    this.animationPlayer.autoplay = autoplay;
    this.$.container.insertBefore(
        this.animationPlayer, this.$.playPauseIconContainer);
    ColorChangeUpdater.forDocument().eventTarget.addEventListener(
        COLOR_PROVIDER_CHANGED, () => this.onColorChange());
  }

  async onColorChange() {
    await this.animationPlayer.refreshAnimationColors();
    this.onPlayingChanged_();
  }

  // Update the URL on the player if one exists, otherwise it will be updated
  // when an instance is created.
  onUrlChanged_() {
    if (this.animationUrl && this.animationPlayer) {
      this.animationPlayer.setAttribute('asset-url', this.animationUrl);
    }
  }

  onPlayingChanged_() {
    if (this.animationPlayer) {
      if (this.playing) {
        this.animationPlayer.play();
      } else {
        this.animationPlayer.pause();
      }
    } else {
      if (this.playing) {
        // Create a player, it will autoplay.
        this.createPlayer(/*autoplay=*/true);
      } else {
        // Nothing to do.
      }
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
