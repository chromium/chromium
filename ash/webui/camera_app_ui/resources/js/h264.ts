// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * H264 related utility functions referenced from
 * media/video/h264_level_limits.cc.
 */

import {assert, assertNotReached} from './assert.js';
import {Resolution} from './type.js';
import {getNumberEnumMapping} from './util.js';

export enum Profile {
  BASELINE = 66,
  MAIN = 77,
  HIGH = 100,
}

export const profileValues =
    new Set(Object.values(getNumberEnumMapping(Profile)));

/**
 * Asserts that a number is one of the value of possible h264 profile.
 */
export function assertProfile(v: number): Profile {
  assert(profileValues.has(v));
  return v;
}

const profileNames: Record<Profile, string> = {
  [Profile.BASELINE]: 'baseline',
  [Profile.MAIN]: 'main',
  [Profile.HIGH]: 'high',
};

/**
 * Gets the name of a h264 profile.
 */
export function getProfileName(profile: Profile): string {
  return profileNames[profile];
}

export enum Level {
  // Ignore unsupported lower levels.
  LV30 = 30,
  LV31 = 31,
  LV32 = 32,
  LV40 = 40,
  LV41 = 41,
  LV42 = 42,
  LV50 = 50,
  LV51 = 51,
  LV52 = 52,
  LV60 = 60,
  LV61 = 61,
  LV62 = 62,
}

export const LEVELS =
    Object.values(getNumberEnumMapping(Level)).sort((a, b) => a - b);

export interface EncoderParameters {
  profile: Profile;
  level: Level;
  bitrate: number;
}

const levelLimits = (() => {
  function limit(processRate: number, frameSize: number, mainBitrate: number) {
    return {processRate, frameSize, mainBitrate};
  }
  return {
    [Level.LV30]: limit(40500, 1620, 10000),
    [Level.LV31]: limit(108000, 3600, 14000),
    [Level.LV32]: limit(216000, 5120, 20000),
    [Level.LV40]: limit(245760, 8192, 20000),
    [Level.LV41]: limit(245760, 8192, 50000),
    [Level.LV42]: limit(522240, 8704, 50000),
    [Level.LV50]: limit(589824, 22080, 135000),
    [Level.LV51]: limit(983040, 36864, 240000),
    [Level.LV52]: limit(2073600, 36864, 240000),
    [Level.LV60]: limit(4177920, 139264, 240000),
    [Level.LV61]: limit(8355840, 139264, 480000),
    [Level.LV62]: limit(16711680, 139264, 800000),
  };
})();

/**
 * @return Returns the maximal available bitrate supported by target
 *     profile and level.
 */
export function getMaxBitrate(profile: Profile, level: Level): number {
  const {mainBitrate} = levelLimits[level];
  const kbs = (() => {
    switch (profile) {
      case Profile.BASELINE:
      case Profile.MAIN:
        return mainBitrate;
      case Profile.HIGH:
        return Math.floor(mainBitrate * 5 / 4);
      default:
        assertNotReached();
    }
  })();
  return kbs * 1000;
}

/**
 * H264 macroblock size in pixels.
 */
const MACROBLOCK_SIZE = 16;

/**
 * @return Returns whether the recording |fps| and |resolution| fit in the h264
 *     level limitation.
 */
export function checkLevelLimits(
    level: Level, fps: number, {width, height}: Resolution): boolean {
  const frameSize =
      Math.ceil(width / MACROBLOCK_SIZE) * Math.ceil(height / MACROBLOCK_SIZE);
  const processRate = frameSize * fps;
  const limit = levelLimits[level];
  return frameSize <= limit.frameSize && processRate <= limit.processRate;
}

/**
 * Gets minimal available level with respect to given profile, bitrate,
 * resolution, fps.
 */
export function getMinimalLevel(
    profile: Profile, bitrate: number, fps: number,
    resolution: Resolution): Level|null {
  for (const level of LEVELS) {
    if (checkLevelLimits(level, fps, resolution) &&
        getMaxBitrate(profile, level) >= bitrate) {
      return level;
    }
  }
  return null;
}
