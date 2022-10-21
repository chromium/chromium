// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'audio-settings' allow users to configure their audio settings in system
 * settings.
 */

import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import '../../icons.html.js';
import '../../settings_shared.css.js';

import {CrSliderElement} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AudioSystemProperties, AudioSystemPropertiesObserverReceiver, CrosAudioConfigInterface, MuteState} from '../../mojom-webui/audio/cros_audio_config.mojom-webui.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface} from '../../router.js';
import {routes} from '../os_route.js';

import {getTemplate} from './audio.html.js';
import {getCrosAudioConfig} from './cros_audio_config.js';

// TODO(crbug/1315757) Remove need to typecast and intersect mixin interfaces
// once RouteObserverMixin is converted to TS
const SettingsAudioElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement)) as {
      new (): PolymerElement & I18nMixinInterface & RouteObserverMixinInterface,
    };

class SettingsAudioElement extends SettingsAudioElementBase {
  static get is() {
    return 'settings-audio';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      crosAudioConfig_: {
        type: Object,
      },

      audioSystemProperties_: {
        type: Object,
      },

      isOutputMuted_: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  private audioSystemProperties_: AudioSystemProperties;
  private audioSystemPropertiesObserverReceiver_:
      AudioSystemPropertiesObserverReceiver;
  private crosAudioConfig_: CrosAudioConfigInterface;
  private isOutputMuted_: boolean;

  constructor() {
    super();
    this.crosAudioConfig_ = getCrosAudioConfig();

    this.audioSystemPropertiesObserverReceiver_ =
        new AudioSystemPropertiesObserverReceiver(this);
  }

  override ready() {
    super.ready();

    this.observeAudioSystemProperties_();
  }

  /**
   * AudioSystemPropertiesObserverInterface override
   */
  onPropertiesUpdated(properties: AudioSystemProperties): void {
    this.audioSystemProperties_ = properties;

    // TODO(crbug.com/1092970): Create and show managed by policy badge if
    // kMutedByPolicy.
    this.isOutputMuted_ =
        this.audioSystemProperties_.outputMuteState !== MuteState.kNotMuted;
  }

  getIsOutputMutedForTest(): boolean {
    return this.isOutputMuted_;
  }

  private observeAudioSystemProperties_(): void {
    this.crosAudioConfig_.observeAudioSystemProperties(
        this.audioSystemPropertiesObserverReceiver_.$
            .bindNewPipeAndPassRemote());
  }

  private isOutputVolumeSliderDisabled_(): boolean {
    return this.audioSystemProperties_.outputMuteState ===
        MuteState.kMutedByPolicy;
  }

  /**
   * Handles the event where the output volume slider is being changed.
   */
  private onOutputVolumeSliderChanged_(): void {
    const sliderValue =
        this.shadowRoot!.querySelector<CrSliderElement>(
                            '#outputVolumeSlider')!.value;
    this.crosAudioConfig_.setOutputVolumePercent(sliderValue);
  }

  // TODO(crbug.com/1092970): Create onOutputMuteTap_ method for setting output
  // mute state.

  // TODO(crbug.com/1092970): Create onOutputDeviceChanged_ method for setting
  // active output device.

  override currentRouteChanged(route: Route) {
    // Does not apply to this page.
    // TODO(crbug.com/1092970): Add DeepLinkingBehavior and attempt deep link.
    if (route !== routes.AUDIO) {
      return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-audio': SettingsAudioElement;
  }
}

customElements.define(SettingsAudioElement.is, SettingsAudioElement);
