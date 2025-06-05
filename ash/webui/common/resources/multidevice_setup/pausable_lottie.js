// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import './icons.html.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {LottieRenderer} from '//resources/cros_components/lottie_renderer/lottie-renderer.js';
import {assert} from '//resources/js/assert.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './pausable_lottie.html.js';

/**
 * Lottie wrapper to allow users to click to pause.  Inspired by
 * <oobe-cr-lottie>
 */
Polymer({
  _template: getTemplate(),
  is: 'pausable-lottie',
  behaviors: [
    I18nBehavior,
  ],
  properties: {
    playing: {
      type: Boolean,
      observer: 'onPlayingChanged',
      value: false,
    },
    animationUrl: {
      type: String,
      observer: 'onUrlChanged',
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
    animationPlayer: {type: LottieRenderer | null, value: null},
  },
  ready() {
    this.addEventListener('click', this.onClick);
    this.addEventListener(
        'cros-lottie-initialized', this.onInitialized, {once: true});
    // Preload the player so that the first frame is shown.
    if (this.preload) {
      this.createPlayer(/*autoplay=*/ true);
    }
  },
  onClick() {
    if (this.hidePlayPauseIcon) {
      return;
    }
    this.playing = !this.playing;
  },
  createPlayer(autoplay = true) {
    this.animationPlayer = document.createElement('cros-lottie-renderer');
    this.animationPlayer.id = 'animation';
    this.animationPlayer.setAttribute('asset-url', this.animationUrl);
    this.animationPlayer.setAttribute('dynamic', String(this.dynamic));
    this.animationPlayer.autoplay = autoplay;
    const container = this.shadowRoot?.querySelector('#container');
    assert(container instanceof HTMLElement);
    const playPauseIconContainer =
        this.shadowRoot?.querySelector('#playPauseIconContainer');
    assert(playPauseIconContainer instanceof HTMLElement);
    container.insertBefore(this.animationPlayer, playPauseIconContainer);
  },
  onUrlChanged() {
    if (this.animationUrl && this.animationPlayer) {
      this.animationPlayer.setAttribute('asset-url', this.animationUrl);
    }
  },
  onPlayingChanged() {
    if (this.animationPlayer) {
      if (this.playing) {
        this.animationPlayer.play();
      } else {
        this.animationPlayer.pause();
      }
    } else if (this.playing) {
      this.createPlayer(/*autoplay=*/ true);
    }
  },
  getIcon(playing) {
    return playing ? 'multidevice-setup-icons-48:pause' :
                     'multidevice-setup-icons-48:play';
  },
  getAria(_locale, playing) {
    return this.i18n(
        playing ? 'pauseAnimationAriaLabel' : 'playAnimationAriaLabel');
  },
});
