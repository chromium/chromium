// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import '../../icons.html.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AudioSystemProperties, AudioSystemPropertiesObserverInterface, AudioSystemPropertiesObserverReceiver, CrosAudioConfigInterface, MuteState} from '../../mojom-webui/audio/cros_audio_config.mojom-webui.js';
import {Route} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {getCrosAudioConfig} from './cros_audio_config.js';

/**
 * @fileoverview
 * 'audio-settings' allow users to configure their audio settings in system
 * settings.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsAudioElementBase =
    mixinBehaviors([RouteObserverBehavior, I18nBehavior], PolymerElement);

/** @polymer */
class SettingsAudioElement extends SettingsAudioElementBase {
  static get is() {
    return 'settings-audio';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected {!CrosAudioConfigInterface} */
      crosAudioConfig_: {
        type: Object,
      },

      /** @protected {!AudioSystemProperties} */
      audioSystemProperties_: {
        type: Object,
      },

      /** @protected */
      isOutputMuted_: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  constructor() {
    super();
    this.crosAudioConfig_ = getCrosAudioConfig();
    /** @private {!AudioSystemPropertiesObserverReceiver} */
    this.audioSystemPropertiesObserverReceiver_ =
        new AudioSystemPropertiesObserverReceiver(
            /**
             * @type {!AudioSystemPropertiesObserverInterface}
             */
            (this));
  }

  /** @override */
  ready() {
    super.ready();
    this.observeAudioSystemProperties_();
  }

  /**
   * AudioSystemPropertiesObserverInterface override
   * @param {!AudioSystemProperties} properties
   */
  onPropertiesUpdated(properties) {
    this.audioSystemProperties_ = properties;

    // TODO(crbug.com/1092970): Create and show managed by policy badge if
    // kMutedByPolicy.
    this.isOutputMuted_ =
        this.audioSystemProperties_.outputMuteState !== MuteState.kNotMuted;
  }

  /**
   * @public
   * @return {boolean}
   */
  getIsOutputMutedForTest() {
    return this.isOutputMuted_;
  }

  /** @protected */
  observeAudioSystemProperties_() {
    this.crosAudioConfig_.observeAudioSystemProperties(
        this.audioSystemPropertiesObserverReceiver_.$
            .bindNewPipeAndPassRemote());
  }

  /**
   * @protected
   * @return {boolean}
   */
  isOutputVolumeSliderDisabled_() {
    return this.audioSystemProperties_.outputMuteState ===
        MuteState.kMutedByPolicy;
  }

  /**
   * Handles the event where the output volume slider is being changed.
   * @private
   */
  onOutputVolumeSliderChanged_() {
    const sliderValue =
        this.shadowRoot.querySelector('#outputVolumeSlider').value;
    this.crosAudioConfig_.setOutputVolumePercent(sliderValue);
  }

  // TODO(crbug.com/1092970): Create onOutputMuteTap_ method for setting output
  // mute state.

  // TODO(crbug.com/1092970): Create onOutputDeviceChanged_ method for setting
  // active output device.

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    // TODO(owenzhang): Add DeepLinkingBehavior and attempt deep link.
    if (route !== routes.AUDIO) {
      return;
    }
  }
}

customElements.define(SettingsAudioElement.is, SettingsAudioElement);
