// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying pausable lottie animation.
 */

import '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import './oobe_icons.html.js';

import {LottieRenderer} from '//resources/cros_components/lottie_renderer/lottie-renderer.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {traceOobeLottieExecution} from '../oobe_trace.js';

import {OobeI18nMixin} from './mixins/oobe_i18n_mixin.js';
import {getTemplate} from './oobe_cr_lottie.html.js';

const OobeCrLottieBase = OobeI18nMixin(PolymerElement);

export class OobeCrLottie extends OobeCrLottieBase {
  static get is() {
    return 'oobe-cr-lottie' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
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
    };
  }

  playing: boolean;
  private animationUrl: string;
  private hidePlayPauseIcon: boolean;
  private preload: boolean;
  private dynamic: boolean;
  private animationPlayer: LottieRenderer|null;

  constructor() {
    super();
    this.animationPlayer = null;
  }

  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick);
    this.addEventListener(
        'cros-lottie-initialized', this.onInitialized, {once: true});
    // Preload the player so that the first frame is shown.
    if (this.preload) {
      this.createPlayer(/*autoplay=*/false);
    }
  }

  private onClick(): void {
    if (this.hidePlayPauseIcon) {
      return;
    }
    this.playing = !this.playing;
  }

  private onInitialized(e: Event): void {
    e.stopPropagation();
    traceOobeLottieExecution();
  }

  /**
   * @param autoplay
   */
  createPlayer(autoplay: boolean = true) {
    this.animationPlayer = document.createElement('cros-lottie-renderer');
    this.animationPlayer.id = 'animation';
    this.animationPlayer.setAttribute('asset-url', this.animationUrl);
    this.animationPlayer.setAttribute('dynamic', String(this.dynamic));
    this.animationPlayer.autoplay = autoplay;

    const container = this.shadowRoot?.querySelector('#container');
    assert(container instanceof HTMLElement);
    const playPauseIconContainer = this.shadowRoot?.
      querySelector('#playPauseIconContainer');
    assert(playPauseIconContainer instanceof HTMLElement);
    container.insertBefore(
        this.animationPlayer, playPauseIconContainer);

    ColorChangeUpdater.forDocument().eventTarget.addEventListener(
        COLOR_PROVIDER_CHANGED, () => this.onColorChange());
  }

  private async onColorChange(): Promise<void> {
    if (this.animationPlayer instanceof LottieRenderer) {
      await this.animationPlayer.refreshAnimationColors();
    }
    this.onPlayingChanged();
  }

  // Update the URL on the player if one exists, otherwise it will be updated
  // when an instance is created.
  private onUrlChanged(): void {
    if (this.animationUrl && this.animationPlayer) {
      this.animationPlayer.setAttribute('asset-url', this.animationUrl);
    }
  }

  private onPlayingChanged(): void {
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

  private getIcon(playing: boolean): string {
    if (playing) {
      return 'oobe-48:pause';
    }
    return 'oobe-48:play';
  }

  private getAria(_locale: string, playing: boolean): string {
    if (playing) {
      return this.i18n('pauseAnimationAriaLabel');
    }
    return this.i18n('playAnimationAriaLabel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeCrLottie.is]: OobeCrLottie;
  }
}

customElements.define(OobeCrLottie.is, OobeCrLottie);
