// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'audio-settings' allow users to configure their audio settings in system
 * settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import '../icons.html.js';
import '../settings_shared.css.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrSliderElement} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {AudioDevice, AudioDeviceType, AudioEffectState, AudioSystemProperties, AudioSystemPropertiesObserverReceiver, MuteState} from '../mojom-webui/cros_audio_config.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {AudioAndCaptionsPageBrowserProxy, AudioAndCaptionsPageBrowserProxyImpl} from '../os_a11y_page/audio_and_captions_page_browser_proxy.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './audio.html.js';
import {CrosAudioConfigInterface, getCrosAudioConfig} from './cros_audio_config.js';
import {FakeCrosAudioConfig} from './fake_cros_audio_config.js';

/** Utility for keeping percent in inclusive range of [0,100].  */
function clampPercent(percent: number): number {
  return Math.max(0, Math.min(percent, 100));
}

const SettingsAudioElementBase = WebUiListenerMixin(DeepLinkingMixin(
    PrefsMixin(RouteObserverMixin(I18nMixin(PolymerElement)))));
const VOLUME_ICON_OFF_LEVEL = 0;
// TODO(b/271871947): Match volume icon logic to QS revamp sliders.
// Matches level calculated in unified_volume_view.cc.
const VOLUME_ICON_LOUD_LEVEL = 34;
const SETTINGS_20PX_ICON_PREFIX = 'settings20:';

export class SettingsAudioElement extends SettingsAudioElementBase {
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

      systemSoundsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('areSystemSoundsEnabled');
        },
        readOnly: true,
      },

      startupSoundEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kChargingSounds,
          Setting.kLowBatterySound,
        ]),
      },

      showAllowAGC: {
        type: Boolean,
        value: loadTimeData.getBoolean('enableForceRespectUiGainsToggle'),
        readonly: true,
      },

      isAllowAGCEnabled: {
        type: Boolean,
        value: true,
        observer: SettingsAudioElement.prototype.onAllowAGCEnabledChanged,
      },
    };
  }

  protected isAllowAGCEnabled: boolean;
  protected showAllowAGC: boolean;

  private audioAndCaptionsBrowserProxy_: AudioAndCaptionsPageBrowserProxy;
  private audioSystemProperties_: AudioSystemProperties;
  private audioSystemPropertiesObserverReceiver_:
      AudioSystemPropertiesObserverReceiver;
  private crosAudioConfig_: CrosAudioConfigInterface;
  private isOutputMuted_: boolean;
  private isInputMuted_: boolean;
  private isNoiseCancellationEnabled_: boolean;
  private isNoiseCancellationSupported_: boolean;
  private outputVolume_: number;
  private systemSoundsEnabled_: boolean;
  private startupSoundEnabled_: boolean;

  constructor() {
    super();
    this.crosAudioConfig_ = getCrosAudioConfig();

    this.audioSystemPropertiesObserverReceiver_ =
        new AudioSystemPropertiesObserverReceiver(this);

    this.audioAndCaptionsBrowserProxy_ =
        AudioAndCaptionsPageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.observeAudioSystemProperties_();

    this.addWebUiListener(
        'startup-sound-setting-retrieved', (startupSoundEnabled: boolean) => {
          this.startupSoundEnabled_ = startupSoundEnabled;
        });
  }

  /**
   * AudioSystemPropertiesObserverInterface override
   */
  onPropertiesUpdated(properties: AudioSystemProperties): void {
    this.audioSystemProperties_ = properties;

    this.isOutputMuted_ =
        this.audioSystemProperties_.outputMuteState !== MuteState.kNotMuted;
    this.isInputMuted_ =
        this.audioSystemProperties_.inputMuteState !== MuteState.kNotMuted;
    const activeInputDevice = this.audioSystemProperties_.inputDevices.find(
        (device: AudioDevice) => device.isActive);
    this.isNoiseCancellationEnabled_ =
        (activeInputDevice?.noiseCancellationState ===
         AudioEffectState.kEnabled);
    this.isNoiseCancellationSupported_ =
        !(activeInputDevice?.noiseCancellationState ===
          AudioEffectState.kNotSupported);
    this.isAllowAGCEnabled =
        (activeInputDevice?.forceRespectUiGainsState ===
         AudioEffectState.kNotEnabled);
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

  /** Determines if audio output is muted by policy. */
  protected isOutputMutedByPolicy_(): boolean {
    return this.audioSystemProperties_.outputMuteState ===
        MuteState.kMutedByPolicy;
  }

  protected onInputMuteClicked(): void {
    this.crosAudioConfig_.setInputMuted(!this.isInputMuted_);
  }

  /** Handles updating active input device. */
  protected onInputDeviceChanged(): void {
    const inputDeviceSelect = this.shadowRoot!.querySelector<HTMLSelectElement>(
        '#audioInputDeviceDropdown');
    assert(!!inputDeviceSelect);
    this.crosAudioConfig_.setActiveDevice(BigInt(inputDeviceSelect.value));
  }

  /** Handles updates to noise cancellation state. */
  protected onNoiseCancellationEnabledChanged(
      enabled: SettingsAudioElement['isNoiseCancellationEnabled_'],
      previousEnabled: SettingsAudioElement['isNoiseCancellationEnabled_']):
      void {
    // Polymer triggers change event on all assignment to
    // `isNoiseCancellationEnabled_` even if the value is logically unchanged.
    // Check previous value before calling `setNoiseCancellationEnabled` to test
    // if value actually updated.
    if (previousEnabled === undefined || previousEnabled === enabled) {
      return;
    }

    this.crosAudioConfig_.setNoiseCancellationEnabled(enabled);
  }

  /** Handles updates to force respect ui gains state. */
  protected onAllowAGCEnabledChanged(
      enabled: SettingsAudioElement['isAllowAGCEnabled'],
      previousEnabled: SettingsAudioElement['isAllowAGCEnabled']): void {
    // Polymer triggers change event on all assignment to
    // `isAllowAGCEnabled` even if the value is logically unchanged.
    // Check previous value before calling `setAllowAGCEnabled` to
    // test if value actually updated.
    if (previousEnabled === undefined || previousEnabled === enabled) {
      return;
    }

    this.crosAudioConfig_.setForceRespectUiGainsEnabled(!enabled);
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
    const outputDeviceSelect =
        this.shadowRoot!.querySelector<HTMLSelectElement>(
            '#audioOutputDeviceDropdown');
    assert(!!outputDeviceSelect);
    this.crosAudioConfig_.setActiveDevice(BigInt(outputDeviceSelect.value));
  }

  /** Handles updating outputMuteState. */
  protected onOutputMuteButtonClicked(): void {
    this.crosAudioConfig_.setOutputMuted(!this.isOutputMuted_);
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    // TODO(crbug.com/1092970): Add DeepLinkingMixin and attempt deep link.
    if (route !== routes.AUDIO) {
      return;
    }

    this.audioAndCaptionsBrowserProxy_.getStartupSoundEnabled();
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

  /**
   * Returns true if input is muted by physical switch; otherwise, return false.
   */
  protected shouldDisableInputGainControls(): boolean {
    return this.audioSystemProperties_.inputMuteState ===
        MuteState.kMutedExternally;
  }

  /** Translates the device name if applicable. */
  private getDeviceName_(audioDevice: AudioDevice): string {
    switch (audioDevice.deviceType) {
      case AudioDeviceType.kHeadphone:
        return this.i18n('audioDeviceHeadphoneLabel');
      case AudioDeviceType.kMic:
        return this.i18n('audioDeviceMicJackLabel');
      case AudioDeviceType.kUsb:
        return this.i18n('audioDeviceUsbLabel', audioDevice.displayName);
      case AudioDeviceType.kBluetooth:
      case AudioDeviceType.kBluetoothNbMic:
        return this.i18n('audioDeviceBluetoothLabel', audioDevice.displayName);
      case AudioDeviceType.kHdmi:
        return this.i18n('audioDeviceHdmiLabel', audioDevice.displayName);
      case AudioDeviceType.kInternalSpeaker:
        return this.i18n('audioDeviceInternalSpeakersLabel');
      case AudioDeviceType.kInternalMic:
        return this.i18n('audioDeviceInternalMicLabel');
      case AudioDeviceType.kFrontMic:
        return this.i18n('audioDeviceFrontMicLabel');
      case AudioDeviceType.kRearMic:
        return this.i18n('audioDeviceRearMicLabel');
      default:
        return audioDevice.displayName;
    }
  }

  /**
   * Returns the appropriate tooltip for output and input device mute buttons
   * based on `muteState`.
   */
  private getMuteTooltip_(muteState: MuteState): string {
    switch (muteState) {
      case MuteState.kNotMuted:
        return this.i18n('audioToggleToMuteTooltip');
      case MuteState.kMutedByUser:
        return this.i18n('audioToggleToUnmuteTooltip');
      case MuteState.kMutedByPolicy:
        return this.i18n('audioMutedByPolicyTooltip');
      case MuteState.kMutedExternally:
        return this.i18n('audioMutedExternallyTooltip');
      default:
        return '';
    }
  }

  /** Returns the appropriate aria-label for input mute button. */
  protected getInputMuteButtonAriaLabel(): string {
    if (this.audioSystemProperties_.inputMuteState ===
        MuteState.kMutedExternally) {
      return this.i18n('audioInputMuteButtonAriaLabelMutedByHardwareSwitch');
    }

    return this.isInputMuted_ ?
        this.i18n('audioInputMuteButtonAriaLabelMuted') :
        this.i18n('audioInputMuteButtonAriaLabelNotMuted');
  }

  /** Returns the appropriate aria-label for output mute button. */
  protected getOutputMuteButtonAriaLabel(): string {
    return this.isOutputMuted_ ?
        this.i18n('audioOutputMuteButtonAriaLabelMuted') :
        this.i18n('audioOutputMuteButtonAriaLabelNotMuted');
  }

  private toggleStartupSoundEnabled_(e: CustomEvent<boolean>): void {
    this.audioAndCaptionsBrowserProxy_.setStartupSoundEnabled(e.detail);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-audio': SettingsAudioElement;
  }
}

customElements.define(SettingsAudioElement.is, SettingsAudioElement);
