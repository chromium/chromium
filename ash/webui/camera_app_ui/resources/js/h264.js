// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * H264 related utility functions referenced from
 * media/video/h264_level_limits.cc.
 */

import {assert, assertNotReached, assertString} from './assert.js';
import {Resolution} from './type.js';  // eslint-disable-line no-unused-vars

/**
 * @enum {number}
 */
export const Profile = {
  BASELINE: 66,
  MAIN: 77,
  HIGH: 100,
};

/**
 * @type {!Set<number>}
 */
const profileValues = new Set(Object.values(Profile));

/**
 * @param {number} v
 * @return {!Profile}
 */
export function assertProfile(v) {
  assert(profileValues.has(v));
  return /** @type {!Profile} */ (v);
}

/**
 * @type {!Map<!Profile, (string|undefined)>}
 */
const profileNames = new Map([
  [Profile.BASELINE, 'baseline'],
  [Profile.MAIN, 'main'],
  [Profile.HIGH, 'high'],
]);

/**
 * @param {!Profile} profile
 * @return {string}
 */
export function getProfileName(profile) {
  return assertString(profileNames.get(profile));
}

/**
 * @enum {number}
 */
export const Level = {
  // Ignore unsupported lower levels.
  LV30: 30,
  LV31: 31,
  LV32: 32,
  LV40: 40,
  LV41: 41,
  LV42: 42,
  LV50: 50,
  LV51: 51,
  LV52: 52,
  LV60: 60,
  LV61: 61,
  LV62: 62,
};

/**
 * @type {!Array<!Level>}
 */
export const Levels = Object.values(Level).sort((a, b) => a - b);

/**
 * @typedef {{
 *   profile: !Profile,
 *   level: !Level,
 *   bitrate: number,
 * }}
 */
export let EncoderParameters;

/**
 * @typedef {{
 *   processRate: number,
 *   frameSize: number,
 *   mainBitrate: number,
 * }}
 */
let LevelLimit;  // eslint-disable-line no-unused-vars

/**
 * @type {!Map<!Level, !LevelLimit>}
 */
const levelLimits = (() => {
  const limit = (processRate, frameSize, mainBitrate) =>
      ({processRate, frameSize, mainBitrate});
  return new Map([
    [Level.LV30, limit(40500, 1620, 10000)],
    [Level.LV31, limit(108000, 3600, 14000)],
    [Level.LV32, limit(216000, 5120, 20000)],
    [Level.LV40, limit(245760, 8192, 20000)],
    [Level.LV41, limit(245760, 8192, 50000)],
    [Level.LV42, limit(522240, 8704, 50000)],
    [Level.LV50, limit(589824, 22080, 135000)],
    [Level.LV51, limit(983040, 36864, 240000)],
    [Level.LV52, limit(2073600, 36864, 240000)],
    [Level.LV60, limit(4177920, 139264, 240000)],
    [Level.LV61, limit(8355840, 139264, 480000)],
    [Level.LV62, limit(16711680, 139264, 800000)],
  ]);
})();

/**
 * @param {!Profile} profile
 * @param {!Level} level
 * @return {number} Returns the maximal available bitrate supported by target
 *     profile and level.
 */
export function getMaxBitrate(profile, level) {
  const {mainBitrate} = levelLimits.get(level);
  const kbs = (() => {
    switch (profile) {
      case Profile.BASELINE:
      case Profile.MAIN:
        return mainBitrate;
      case Profile.HIGH:
        return Math.floor(mainBitrate * 5 / 4);
    }
    assertNotReached();
  })();
  return kbs * 1000;
}

/**
 * H264 macroblock size in pixels.
 */
const MACROBLOCK_SIZE = 16;

/**
 * @param {!Level} level
 * @param {number} fps
 * @param {!Resolution} resolution
 * @return {boolean} Returns whether the recording |fps| and |resolution| fit in
 *     the h264 level limitation.
 */
export function checkLevelLimits(level, fps, {width, height}) {
  const frameSize =
      Math.ceil(width / MACROBLOCK_SIZE) * Math.ceil(height / MACROBLOCK_SIZE);
  const processRate = frameSize * fps;
  const limit = levelLimits.get(level);
  return frameSize <= limit.frameSize && processRate <= limit.processRate;
}

/**
 * Gets minimal available level with respect to given profile, bitrate,
 * resolution, fps.
 * @param {!Profile} profile
 * @param {number} bitrate
 * @param {number} fps
 * @param {!Resolution} resolution
 * @return {?Level}
 */
export function getMinimalLevel(profile, bitrate, fps, resolution) {
  for (const level of Levels) {
    if (checkLevelLimits(level, fps, resolution) &&
        getMaxBitrate(profile, level) >= bitrate) {
      return level;
    }
  }
  return null;
}
