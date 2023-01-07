// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility for converting frequencies into their related channel.
 */

import {ChannelBand, ChannelProperties} from './diagnostics_types.js';

/**
 * Map of keyed on center frequency with values of related channel for 5GHz.
 * Not all channels are used in all countries. Channels which are not used in
 * any country have been excluded.
 */
const CHANNELS: Map<number, ChannelProperties> = new Map([
  [2412, {channel: 1, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2417, {channel: 2, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2422, {channel: 3, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2427, {channel: 4, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2432, {channel: 5, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2437, {channel: 6, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2442, {channel: 7, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2447, {channel: 8, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2452, {channel: 9, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2457, {channel: 10, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2462, {channel: 11, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2467, {channel: 12, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2472, {channel: 13, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [2484, {channel: 14, band: ChannelBand.TWO_DOT_FOUR_GHZ}],
  [5160, {channel: 32, band: ChannelBand.FIVE_GHZ}],
  [5170, {channel: 34, band: ChannelBand.FIVE_GHZ}],
  [5180, {channel: 36, band: ChannelBand.FIVE_GHZ}],
  [5190, {channel: 38, band: ChannelBand.FIVE_GHZ}],
  [5200, {channel: 40, band: ChannelBand.FIVE_GHZ}],
  [5210, {channel: 42, band: ChannelBand.FIVE_GHZ}],
  [5220, {channel: 44, band: ChannelBand.FIVE_GHZ}],
  [5230, {channel: 46, band: ChannelBand.FIVE_GHZ}],
  [5240, {channel: 48, band: ChannelBand.FIVE_GHZ}],
  [5250, {channel: 50, band: ChannelBand.FIVE_GHZ}],
  [5260, {channel: 52, band: ChannelBand.FIVE_GHZ}],
  [5270, {channel: 54, band: ChannelBand.FIVE_GHZ}],
  [5280, {channel: 56, band: ChannelBand.FIVE_GHZ}],
  [5290, {channel: 58, band: ChannelBand.FIVE_GHZ}],
  [5300, {channel: 60, band: ChannelBand.FIVE_GHZ}],
  [5310, {channel: 62, band: ChannelBand.FIVE_GHZ}],
  [5320, {channel: 64, band: ChannelBand.FIVE_GHZ}],
  [5340, {channel: 68, band: ChannelBand.FIVE_GHZ}],
  [5480, {channel: 96, band: ChannelBand.FIVE_GHZ}],
  [5500, {channel: 100, band: ChannelBand.FIVE_GHZ}],
  [5510, {channel: 102, band: ChannelBand.FIVE_GHZ}],
  [5520, {channel: 104, band: ChannelBand.FIVE_GHZ}],
  [5530, {channel: 106, band: ChannelBand.FIVE_GHZ}],
  [5540, {channel: 108, band: ChannelBand.FIVE_GHZ}],
  [5550, {channel: 110, band: ChannelBand.FIVE_GHZ}],
  [5560, {channel: 112, band: ChannelBand.FIVE_GHZ}],
  [5570, {channel: 114, band: ChannelBand.FIVE_GHZ}],
  [5580, {channel: 116, band: ChannelBand.FIVE_GHZ}],
  [5590, {channel: 118, band: ChannelBand.FIVE_GHZ}],
  [5600, {channel: 120, band: ChannelBand.FIVE_GHZ}],
  [5610, {channel: 122, band: ChannelBand.FIVE_GHZ}],
  [5620, {channel: 124, band: ChannelBand.FIVE_GHZ}],
  [5630, {channel: 126, band: ChannelBand.FIVE_GHZ}],
  [5640, {channel: 128, band: ChannelBand.FIVE_GHZ}],
  [5660, {channel: 132, band: ChannelBand.FIVE_GHZ}],
  [5670, {channel: 134, band: ChannelBand.FIVE_GHZ}],
  [5680, {channel: 136, band: ChannelBand.FIVE_GHZ}],
  [5690, {channel: 138, band: ChannelBand.FIVE_GHZ}],
  [5700, {channel: 140, band: ChannelBand.FIVE_GHZ}],
  [5710, {channel: 142, band: ChannelBand.FIVE_GHZ}],
  [5720, {channel: 144, band: ChannelBand.FIVE_GHZ}],
  [5745, {channel: 149, band: ChannelBand.FIVE_GHZ}],
  [5755, {channel: 151, band: ChannelBand.FIVE_GHZ}],
  [5765, {channel: 153, band: ChannelBand.FIVE_GHZ}],
  [5775, {channel: 155, band: ChannelBand.FIVE_GHZ}],
  [5785, {channel: 157, band: ChannelBand.FIVE_GHZ}],
  [5795, {channel: 159, band: ChannelBand.FIVE_GHZ}],
  [5805, {channel: 161, band: ChannelBand.FIVE_GHZ}],
  [5815, {channel: 163, band: ChannelBand.FIVE_GHZ}],
  [5825, {channel: 165, band: ChannelBand.FIVE_GHZ}],
  [5835, {channel: 167, band: ChannelBand.FIVE_GHZ}],
  [5845, {channel: 169, band: ChannelBand.FIVE_GHZ}],
  [5855, {channel: 171, band: ChannelBand.FIVE_GHZ}],
  [5865, {channel: 173, band: ChannelBand.FIVE_GHZ}],
  [5875, {channel: 175, band: ChannelBand.FIVE_GHZ}],
  [5885, {channel: 177, band: ChannelBand.FIVE_GHZ}],
  [5900, {channel: 180, band: ChannelBand.FIVE_GHZ}],
  [5910, {channel: 182, band: ChannelBand.FIVE_GHZ}],
  [5915, {channel: 183, band: ChannelBand.FIVE_GHZ}],
  [5920, {channel: 184, band: ChannelBand.FIVE_GHZ}],
  [5935, {channel: 187, band: ChannelBand.FIVE_GHZ}],
  [5940, {channel: 188, band: ChannelBand.FIVE_GHZ}],
  [5945, {channel: 189, band: ChannelBand.FIVE_GHZ}],
  [5960, {channel: 192, band: ChannelBand.FIVE_GHZ}],
  [5980, {channel: 196, band: ChannelBand.FIVE_GHZ}],
]);

/**
 * Determines channel based on |frequency| which is expected to match the
 * channel center frequency.  In all other cases return null.
 */
export function convertFrequencyToChannel(frequency: number): number|null {
  const channelProperties: ChannelProperties|undefined =
      CHANNELS.get(frequency);
  return channelProperties ? channelProperties.channel : null;
}

export function getFrequencyChannelBand(frequency: number): ChannelBand {
  const channelProperties: ChannelProperties|undefined =
      CHANNELS.get(frequency);
  return channelProperties ? channelProperties.band : ChannelBand.UNKNOWN;
}
