// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReadonlySignal, signal} from './reactive/signal.js';

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

async function listAllMicrophones(infoCallback: MicInfoCallback
): Promise<MicrophoneInfo[]> {
  const allDevices = await navigator.mediaDevices.enumerateDevices();

  // Use only audioinput, and remove the device with the deviceId "default"
  // since it's a duplicated entry and can't be used to get the internal info.
  const devices = allDevices.filter(
    (d) => d.kind === 'audioinput' && d.deviceId !== DEFAULT_MIC_ID
  );

  // Retrieve the internal info from mojo.
  const devicesWithInfo =
    await Promise.all(devices.map(async ({deviceId, label}) => {
      const internalMicInfo = await infoCallback(deviceId);
      return {...internalMicInfo, deviceId, label};
    }));

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

export class MicrophoneManager {
  private readonly cachedMicrophoneList = signal<MicrophoneInfo[]>([]);

  static async create(infoCallback: MicInfoCallback
  ): Promise<MicrophoneManager> {
    const microphoneList = await listAllMicrophones(infoCallback);
    return new MicrophoneManager(microphoneList);
  }

  private constructor(microphoneList: MicrophoneInfo[]) {
    this.cachedMicrophoneList.value = microphoneList;
  }

  getMicrophoneList(): ReadonlySignal<MicrophoneInfo[]> {
    return this.cachedMicrophoneList;
  }
}
