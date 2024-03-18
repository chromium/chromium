// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.audio API
 * Generated from: extensions/common/api/audio.idl
 * run `tools/json_schema_compiler/compiler.py extensions/common/api/audio.idl
 * -g ts_definitions` to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace audio {

      export enum StreamType {
        INPUT = 'INPUT',
        OUTPUT = 'OUTPUT',
      }

      export enum DeviceType {
        HEADPHONE = 'HEADPHONE',
        MIC = 'MIC',
        USB = 'USB',
        BLUETOOTH = 'BLUETOOTH',
        HDMI = 'HDMI',
        INTERNAL_SPEAKER = 'INTERNAL_SPEAKER',
        INTERNAL_MIC = 'INTERNAL_MIC',
        FRONT_MIC = 'FRONT_MIC',
        REAR_MIC = 'REAR_MIC',
        KEYBOARD_MIC = 'KEYBOARD_MIC',
        HOTWORD = 'HOTWORD',
        LINEOUT = 'LINEOUT',
        POST_MIX_LOOPBACK = 'POST_MIX_LOOPBACK',
        POST_DSP_LOOPBACK = 'POST_DSP_LOOPBACK',
        ALSA_LOOPBACK = 'ALSA_LOOPBACK',
        OTHER = 'OTHER',
      }

      export interface AudioDeviceInfo {
        id: string;
        streamType: StreamType;
        deviceType: DeviceType;
        displayName: string;
        deviceName: string;
        isActive: boolean;
        level: number;
        stableDeviceId?: string;
      }

      export interface DeviceFilter {
        streamTypes?: StreamType[];
        isActive?: boolean;
      }

      export interface DeviceProperties {
        level?: number;
      }

      export interface DeviceIdLists {
        input?: string[];
        output?: string[];
      }

      export interface MuteChangedEvent {
        streamType: StreamType;
        isMuted: boolean;
      }

      export interface LevelChangedEvent {
        deviceId: string;
        level: number;
      }

      export function getDevices(
          filter?: DeviceFilter,
          callback?: (devices: AudioDeviceInfo[]) => void): void;

      export function setActiveDevices(
          ids: DeviceIdLists, callback: () => void): void;

      export function setProperties(
          id: string, properties: DeviceProperties, callback: () => void): void;

      export function getMute(
          streamType: StreamType, callback: (isMute: boolean) => void): void;

      export function setMute(
          streamType: StreamType, isMuted: boolean, callback: () => void): void;

      export const onLevelChanged:
          ChromeEvent<(event: LevelChangedEvent) => void>;

      export const onMuteChanged:
          ChromeEvent<(event: MuteChangedEvent) => void>;

      export const onDeviceListChanged:
          ChromeEvent<(devices: AudioDeviceInfo[]) => void>;

    }
  }
}
