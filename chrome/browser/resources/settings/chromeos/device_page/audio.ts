// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'audio-settings' allow users to configure their audio settings in system
 * settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import '../../icons.html.js';
import '../../settings_shared.css.js';

import {CrSliderElement} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AudioSystemPropertiesObserverReceiver, MuteState} from '../../mojom-webui/audio/cros_audio_config.mojom-webui.js';
import {routes} from '../os_route.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {getTemplate} from './audio.html.js';
import {CrosAudioConfigInterface, getCrosAudioConfig} from './cros_audio_config.js';
// TODO(b/260277007): Update import to get `AudioSystemProperties` from
// `cros_audio_config.mojom-webui.js` once mojo updated to handle audio input.
import {AudioDevice, AudioEffectState, AudioSystemProperties, FakeCrosAudioConfig} from './fake_cros_audio_config.js';

/** Utility for keeping percent in inclusive range of [0,100].  */
function clampPercent(percent: number): number {
  return Math.max(0, Math.min(percent, 100));
}

const SettingsAudioElementBase = RouteObserverMixin(I18nMixin(PolymerElement));
const VOLUME_ICON_OFF_LEVEL = 0;
const VOLUME_ICON_LOUD_LEVEL = 30;
const SETTINGS_20PX_ICON_PREFIX = 'settings20:';

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

      isInputMuted_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      isNoiseCancellationEnabled_: {
        type: Boolean,
        observer:
            SettingsAudioElement.prototype.onNoiseCancellationEnabledChanged,
      },

      isNoiseCancellationSupported_: {
        type: Boolean,
      },

      outputVolume_: {
        type: Number,
      },
    };
  }

  private audioSystemProperties_: AudioSystemProperties;
  private audioSystemPropertiesObserverReceiver_:
      AudioSystemPropertiesObserverReceiver;
  private crosAudioConfig_: CrosAudioConfigInterface;
  private isOutputMuted_: boolean;
  private isInputMuted_: boolean;
  private isNoiseCancellationEnabled_: boolean;
  private isNoiseCancellationSupported_: boolean;
  private outputVolume_: number;

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
    this.isInputMuted_ =
        this.audioSystemProperties_.inputMuteState !== MuteState.kNotMuted;
    const activeInputDevice = this.audioSystemProperties_.inputDevices.find(
        (device: AudioDevice) => device.isActive);
    this.isNoiseCancellationEnabled_ =
        (activeInputDevice?.noiseCancellationState ===
         AudioEffectState.ENABLED);
    this.isNoiseCancellationSupported_ =
        !(activeInputDevice?.noiseCancellationState ===
          AudioEffectState.NOT_SUPPORTED);
    this.outputVolume_ = this.audioSystemProperties_.outputVolumePercent;
  }

  getIsOutputMutedForTest(): boolean {
    return this.isOutputMuted_;
  }

  getIsInputMutedForTest(): boolean {
    return this.isInputMuted_;
  }

  private observeAudioSystemProperties_(): void {
    // Use fake observer implementation to access additional properties not
    // available on mojo interface.
    if (this.crosAudioConfig_ instanceof FakeCrosAudioConfig) {
      this.crosAudioConfig_.observeAudioSystemProperties(this);
      return;
    }

    this.crosAudioConfig_.observeAudioSystemProperties(
        this.audioSystemPropertiesObserverReceiver_.$
            .bindNewPipeAndPassRemote());
  }

  private isOutputVolumeSliderDisabled_(): boolean {
    return this.audioSystemProperties_.outputMuteState ===
        MuteState.kMutedByPolicy;
  }

  protected onInputMuteClicked(): void {
    // TODO(b/260277007): Remove condition when setInputMuted added to mojo
    // definition.
    if (!this.crosAudioConfig_.setInputMuted) {
      return;
    }
    this.crosAudioConfig_.setInputMuted(!this.isInputMuted_);
  }

  /** Handles updating active input device. */
  protected onInputDeviceChanged(): void {
    // TODO(b/260277007): Remove condition when setActiveDevice added to mojo
    // definition.
    if (!this.crosAudioConfig_.setActiveDevice) {
      return;
    }
    const inputDeviceSelect = this.shadowRoot!.querySelector<HTMLSelectElement>(
        '#audioInputDeviceDropdown');
    assert(!!inputDeviceSelect);
    this.crosAudioConfig_.setActiveDevice(BigInt(inputDeviceSelect.value));
  }

  /** Handles updates to noise cancellation state. */
  protected onNoiseCancellationEnabledChanged(
      enabled: SettingsAudioElement['isNoiseCancellationEnabled_']): void {
    // TODO(b/260277007): Remove condition when setActiveDevice added to mojo
    // definition.
    if (!this.crosAudioConfig_.setNoiseCancellationEnabled) {
      return;
    }

    this.crosAudioConfig_.setNoiseCancellationEnabled(enabled);
  }

  /**
   * Handles the event where the input volume slider is being changed.
   */
  protected onInputVolumeSliderChanged(): void {
    const sliderValue = this.shadowRoot!
                            .querySelector<CrSliderElement>(
                                '#audioInputGainVolumeSlider')!.value;
    this.crosAudioConfig_.setInputGainPercent(clampPercent(sliderValue));
  }

  /**
   * Handles the event where the output volume slider is being changed.
   */
  private onOutputVolumeSliderChanged_(): void {
    const sliderValue =
        this.shadowRoot!.querySelector<CrSliderElement>(
                            '#outputVolumeSlider')!.value;
    this.crosAudioConfig_.setOutputVolumePercent(clampPercent(sliderValue));
  }

  /** Handles updating active output device. */
  protected onOutputDeviceChanged(): void {
    // TODO(b/260277007): Remove condition when setActiveDevice added to mojo
    // definition.
    if (!this.crosAudioConfig_.setActiveDevice) {
      return;
    }
    const outputDeviceSelect =
        this.shadowRoot!.querySelector<HTMLSelectElement>(
            '#audioOutputDeviceDropdown');
    assert(!!outputDeviceSelect);
    this.crosAudioConfig_.setActiveDevice(BigInt(outputDeviceSelect.value));
  }

  /** Handles updating outputMuteState. */
  protected onOutputMuteButtonClicked(): void {
    // TODO(b/260277007): Remove condition when setOutputMuted added to mojo
    // definition.
    if (!this.crosAudioConfig_.setOutputMuted) {
      return;
    }
    this.crosAudioConfig_.setOutputMuted(!this.isOutputMuted_);
  }

  override currentRouteChanged(route: Route) {
    // Does not apply to this page.
    // TODO(crbug.com/1092970): Add DeepLinkingMixin and attempt deep link.
    if (route !== routes.AUDIO) {
      return;
    }
  }

  /** Handles updating the mic icon depending on the input mute state. */
  protected getInputIcon_(): string {
    return this.isInputMuted_ ? 'settings:mic-off' : 'cr:mic';
  }

  /**
   * Handles updating the output icon depending on the output mute state and
   * volume.
   */
  protected getOutputIcon_(): string {
    if (this.isOutputMuted_) {
      return SETTINGS_20PX_ICON_PREFIX + 'volume-up-off';
    }

    if (this.outputVolume_ === VOLUME_ICON_OFF_LEVEL) {
      return SETTINGS_20PX_ICON_PREFIX + 'volume-zero';
    }

    if (this.outputVolume_ < VOLUME_ICON_LOUD_LEVEL) {
      return SETTINGS_20PX_ICON_PREFIX + 'volume-down';
    }

    return SETTINGS_20PX_ICON_PREFIX + 'volume-up';
  }

  /**
   * Handles the case when there are no output devices. The output section
   * should be hidden in this case.
   */
  protected getOutputHidden_(): boolean {
    return this.audioSystemProperties_.outputDevices.length === 0;
  }

  /**
   * Handles the case when there are no input devices. The input section should
   * be hidden in this case.
   */
  protected getInputHidden_(): boolean {
    return this.audioSystemProperties_.inputDevices.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-audio': SettingsAudioElement;
  }
}

customElements.define(SettingsAudioElement.is, SettingsAudioElement);
