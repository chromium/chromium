// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReadonlySignal, signal} from './reactive/signal.js';
import {assert, assertExists} from './utils/assert.js';
import {AsyncJobQueue} from './utils/async_job_queue.js';

const DEFAULT_MIC_ID = 'default';

export interface InternalMicInfo {
  // Whether the microphone is the system default microphone.
  isDefault: boolean;
  // Whether the microphone is an internal microphone.
  isInternal: boolean;
}

// A subset of info from MediaDeviceInfo.
export interface BasicMicInfo {
  deviceId: string;
  label: string;
}

export type MicrophoneInfo = BasicMicInfo&InternalMicInfo;

type MicInfoCallback = (deviceId: string) => Promise<InternalMicInfo>;

async function listAllMicrophones(
  infoCallback: MicInfoCallback,
): Promise<MicrophoneInfo[]> {
  const allDevices = await navigator.mediaDevices.enumerateDevices();

  // Use only audioinput, and remove the device with the deviceId "default"
  // since it's a duplicated entry and can't be used to get the internal info.
  const devices = allDevices.filter(
    (d) => d.kind === 'audioinput' && d.deviceId !== DEFAULT_MIC_ID,
  );

  // Retrieve the internal info from mojo.
  const devicesWithInfo = await Promise.all(
    devices.map(async ({deviceId, label}) => {
      const internalMicInfo = await infoCallback(deviceId);
      return {...internalMicInfo, deviceId, label};
    }),
  );

  // Microphones sorting order: Default mic, Internal mics, then by label.
  const sortedDevices = devicesWithInfo.sort((a, b) => {
    if (a.isDefault !== b.isDefault) {
      return a.isDefault ? -1 : 1;
    } else if (a.isInternal !== b.isInternal) {
      return a.isInternal ? -1 : 1;
    }
    return a.label.localeCompare(b.label);
  });

  return sortedDevices;
}

/**
 * Returns whether the given `micId` is a valid microphone deviceId.
 *
 * @param microphones List of all connected microphones.
 * @param micId DeviceId of the microphone.
 * @return Whether the given `micId` is a valid microphone deviceId.
 */
function isValidMicId(
  microphones: MicrophoneInfo[],
  micId: string|null,
): boolean {
  return microphones.some((device) => device.deviceId === micId);
}

/**
 * Return the deviceId of the selected microphone.
 *
 * In case the latest selected microphone is still connected, returns
 * `currentMicId`, otherwise, returns the deviceId of the default microphone.
 *
 * @param microphoneList List of all connected microphones.
 * @param currentMicId DeviceId of the current selected microphone.
 * @return DeviceId of the selected microphone.
 */
function getSelectedMicId(
  microphoneList: MicrophoneInfo[],
  currentMicId: string|null,
): string|null {
  if (isValidMicId(microphoneList, currentMicId)) {
    return currentMicId;
  }

  // TODO(kamchonlathorn): Handle the case when there are no microphones,
  // probably show an error dialog.
  if (microphoneList.length === 0) {
    console.error('There are no connected microphones.');
    return '';
  }

  // In case the microphone is unplugged, fall back to the default device.
  return assertExists(microphoneList[0]).deviceId;
}

export class MicrophoneManager {
  private readonly cachedMicrophoneList = signal<MicrophoneInfo[]>([]);

  private readonly selectedMicId = signal<string|null>(null);

  private readonly updateMicListQueue = new AsyncJobQueue('keepLatest');

  static async create(
    infoCallback: MicInfoCallback,
  ): Promise<MicrophoneManager> {
    const microphoneList = await listAllMicrophones(infoCallback);
    return new MicrophoneManager(microphoneList, infoCallback);
  }

  private constructor(
    microphoneList: MicrophoneInfo[],
    private readonly infoCallback: MicInfoCallback,
  ) {
    this.cachedMicrophoneList.value = microphoneList;
    this.selectedMicId.value = getSelectedMicId(microphoneList, null);
    navigator.mediaDevices.addEventListener('devicechange', () => {
      this.updateMicListQueue.push(() => this.updateActiveMicrophones());
    });
  }

  getMicrophoneList(): ReadonlySignal<MicrophoneInfo[]> {
    return this.cachedMicrophoneList;
  }

  getSelectedMicId(): ReadonlySignal<string|null> {
    return this.selectedMicId;
  }

  setSelectedMicId(micId: string): void {
    assert(
      isValidMicId(this.cachedMicrophoneList.value, micId),
      `Invalid microphone deviceId: ${micId}`,
    );
    this.selectedMicId.value = micId;
  }

  private async updateActiveMicrophones(): Promise<void> {
    const microphones = await listAllMicrophones(this.infoCallback);
    this.cachedMicrophoneList.value = microphones;
    this.selectedMicId.value = getSelectedMicId(
      microphones,
      this.selectedMicId.value,
    );
  }
}
