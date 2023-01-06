// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Fake implementation of CrosAudioConfig for testing.
 */

import {assert} from 'chrome://resources/js/assert_ts.js';

import {AudioDevice as AudioDeviceMojom, AudioDeviceType, AudioSystemProperties as AudioSystemPropertiesMojom, CrosAudioConfigInterface, MuteState} from '../../mojom-webui/audio/cros_audio_config.mojom-webui.js';

// TODO(b/260277007): Remove enum and update imports when mojo updated with
// AudioEffectState.
export enum AudioEffectState {
  NOT_SUPPORTED,
  NOT_ENABLED,
  ENABLED,
}

// TODO(b/260277007): Remove type alias and unused types when mojo updated to
// handle noise cancellation.
export interface FakeAudioDevice extends AudioDeviceMojom {
  noiseCancellationState: AudioEffectState;
}

export type AudioDevice = AudioDeviceMojom&Partial<FakeAudioDevice>;

export const defaultFakeMicJack: AudioDevice = {
  id: BigInt(1),
  displayName: 'Mic Jack',
  isActive: true,
  deviceType: AudioDeviceType.kInternalMic,
  noiseCancellationState: AudioEffectState.NOT_SUPPORTED,
};

export const fakeSpeakerActive: AudioDevice = {
  id: BigInt(2),
  displayName: 'Speaker',
  isActive: true,
  deviceType: AudioDeviceType.kInternalSpeaker,
  noiseCancellationState: AudioEffectState.NOT_SUPPORTED,
};

export const fakeMicJackInactive: AudioDevice = {
  id: BigInt(3),
  displayName: 'Mic Jack',
  isActive: false,
  deviceType: AudioDeviceType.kInternalSpeaker,
  noiseCancellationState: AudioEffectState.NOT_SUPPORTED,
};

export const defaultFakeSpeaker: AudioDevice = {
  id: BigInt(4),
  displayName: 'Speaker',
  isActive: false,
  deviceType: AudioDeviceType.kInternalSpeaker,
  noiseCancellationState: AudioEffectState.NOT_SUPPORTED,
};

export const fakeInternalFrontMic: AudioDevice = {
  id: BigInt(5),
  displayName: 'FrontMic',
  isActive: true,
  deviceType: AudioDeviceType.kFrontMic,
  noiseCancellationState: AudioEffectState.NOT_ENABLED,
};

export const fakeBluetoothMic: AudioDevice = {
  id: BigInt(6),
  displayName: 'Bluetooth Mic',
  isActive: false,
  deviceType: AudioDeviceType.kBluetoothNbMic,
  noiseCancellationState: AudioEffectState.NOT_SUPPORTED,
};

export const fakeInternalMicActive: AudioDevice = {
  id: BigInt(7),
  displayName: 'Internal Mic',
  isActive: true,
  deviceType: AudioDeviceType.kInternalMic,
  noiseCancellationState: AudioEffectState.NOT_SUPPORTED,
};

// TODO(b/260277007): Remove type alias and unused types when mojo updated to
// handle audio input.
export interface FakeAudioSystemProperties extends AudioSystemPropertiesMojom {
  inputDevices: AudioDevice[];
}

export type AudioSystemProperties =
    AudioSystemPropertiesMojom&Partial<FakeAudioSystemProperties>;

export interface FakePropertiesObserverInterface {
  onPropertiesUpdated(properties: AudioSystemProperties): void;
}

export const defaultFakeAudioSystemProperties: AudioSystemProperties = {
  outputDevices: [defaultFakeSpeaker, defaultFakeMicJack],
  outputVolumePercent: 75,
  inputGainPercent: 87,
  outputMuteState: MuteState.kNotMuted,
  inputDevices: [fakeInternalFrontMic, fakeBluetoothMic],
  inputMuteState: MuteState.kNotMuted,
};

/** Creates an audio device based on provided device and isActive override. */
export function createAudioDevice(
    baseDevice: AudioDevice, isActive: boolean): AudioDevice {
  assert(!!baseDevice);
  return ({...baseDevice, isActive});
}

export interface FakeCrosAudioConfigInterface extends CrosAudioConfigInterface {
  setInputGainPercent(percent: number): void;
  setNoiseCancellationEnabled(enabled: boolean): void;
}

export class FakeCrosAudioConfig implements FakeCrosAudioConfigInterface {
  private audioSystemProperties: AudioSystemProperties =
      defaultFakeAudioSystemProperties;
  private observers: FakePropertiesObserverInterface[] = [];

  observeAudioSystemProperties(observer: FakePropertiesObserverInterface):
      void {
    this.observers.push(observer);
    this.notifyAudioSystemPropertiesUpdated();
  }

  /**
   * Sets the active output or input device and notifies observers.
   */
  setActiveDevice(deviceId: bigint): void {
    const isOutputDevice: boolean =
        !!(this.audioSystemProperties.outputDevices.find(
            (device: AudioDevice) => device.id === deviceId));
    if (isOutputDevice) {
      const devices = this.audioSystemProperties.outputDevices.map(
          (device: AudioDevice): AudioDevice =>
              createAudioDevice(device, device.id === deviceId));
      this.audioSystemProperties.outputDevices = devices;
    } else {
      // Device must be an input device otherwise an invalid device was
      // provided.
      assert(this.audioSystemProperties.inputDevices.find(
          (device: AudioDevice) => device.id === deviceId));
      const devices = this.audioSystemProperties.inputDevices.map(
          (device: AudioDevice): AudioDevice =>
              createAudioDevice(device, device.id === deviceId));
      this.audioSystemProperties.inputDevices = devices;
    }
    this.notifyAudioSystemPropertiesUpdated();
  }

  /**
   * Sets `audioSystemProperties` to the desired properties and notifies
   * observers.
   */
  setAudioSystemProperties(properties: AudioSystemProperties): void {
    this.audioSystemProperties = properties;
    this.notifyAudioSystemPropertiesUpdated();
  }

  /** Handle updating active input device noise cancellation state. */
  setNoiseCancellationEnabled(enabled: boolean): void {
    if (!this.audioSystemProperties.inputDevices) {
      return;
    }

    const activeIndex = this.audioSystemProperties.inputDevices.findIndex(
        (device: AudioDevice) => device.isActive &&
            device.noiseCancellationState !== AudioEffectState.NOT_SUPPORTED);
    if (activeIndex === -1) {
      return;
    }
    const nextState: AudioEffectState =
        enabled ? AudioEffectState.ENABLED : AudioEffectState.NOT_ENABLED;
    this.audioSystemProperties.inputDevices[activeIndex]!
        .noiseCancellationState = nextState;
    this.notifyAudioSystemPropertiesUpdated();
  }

  /**
   * Sets the mute state based on provided value.
   */
  setOutputMuted(muted: boolean): void {
    this.audioSystemProperties.outputMuteState =
        muted ? MuteState.kMutedByUser : MuteState.kNotMuted;
    this.notifyAudioSystemPropertiesUpdated();
  }

  /**
   * Sets the input device mute state to `kMutedByUser` when true and
   * `kNotMuted` when false.
   */
  setInputMuted(muted: boolean): void {
    const muteState = muted ? MuteState.kMutedByUser : MuteState.kNotMuted;
    this.audioSystemProperties.inputMuteState = muteState;
    this.notifyAudioSystemPropertiesUpdated();
  }

  /**
   * Sets the `inputGainPercent` to the desired volume and notifies
   * observers.
   */
  setInputGainPercent(gain: number): void {
    assert(gain >= 0 && gain <= 100);
    this.audioSystemProperties.inputGainPercent = gain;
    this.notifyAudioSystemPropertiesUpdated();
  }

  /**
   * Sets the `outputVolumePercent` to the desired volume and notifies
   * observers.
   */
  setOutputVolumePercent(volume: number): void {
    this.audioSystemProperties.outputVolumePercent = volume;
    this.notifyAudioSystemPropertiesUpdated();
  }

  /** Notifies the observer list that `audioSystemProperties` has changed. */
  private notifyAudioSystemPropertiesUpdated(): void {
    this.observers.forEach((observer: FakePropertiesObserverInterface) => {
      observer.onPropertiesUpdated(this.audioSystemProperties);
    });
  }
}
