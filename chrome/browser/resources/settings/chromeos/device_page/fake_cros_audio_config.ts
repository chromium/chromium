// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Fake implementation of CrosAudioConfig for testing.
 */

import {AudioDevice, AudioDeviceType, AudioSystemProperties, AudioSystemPropertiesObserverInterface, CrosAudioConfigInterface, MuteState} from '../../mojom-webui/audio/cros_audio_config.mojom-webui.js';

export const defaultFakeMicJack: AudioDevice = {
  id: BigInt(1),
  displayName: 'Mic Jack',
  isActive: true,
  deviceType: AudioDeviceType.kInternalMic,
};

export const fakeSpeakerActive: AudioDevice = {
  id: BigInt(2),
  displayName: 'Speaker',
  isActive: true,
  deviceType: AudioDeviceType.kInternalSpeaker,
};

export const fakeMicJackInactive: AudioDevice = {
  id: BigInt(3),
  displayName: 'Mic Jack',
  isActive: false,
  deviceType: AudioDeviceType.kInternalSpeaker,
};

export const defaultFakeSpeaker: AudioDevice = {
  id: BigInt(4),
  displayName: 'Speaker',
  isActive: false,
  deviceType: AudioDeviceType.kInternalSpeaker,
};

export const defaultFakeAudioSystemProperties: AudioSystemProperties = {
  outputDevices: [defaultFakeSpeaker, defaultFakeMicJack],
  outputVolumePercent: 75,
  outputMuteState: MuteState.kNotMuted,
};

export class FakeCrosAudioConfig implements CrosAudioConfigInterface {
  private audioSystemProperties: AudioSystemProperties =
      defaultFakeAudioSystemProperties;
  private observers: AudioSystemPropertiesObserverInterface[] = [];

  observeAudioSystemProperties(
      observer: AudioSystemPropertiesObserverInterface): void {
    this.observers.push(observer);
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
    this.observers.forEach(
        (observer: AudioSystemPropertiesObserverInterface) => {
          observer.onPropertiesUpdated(this.audioSystemProperties);
        });
  }
}
