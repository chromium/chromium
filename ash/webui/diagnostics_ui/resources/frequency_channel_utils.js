// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility for converting frequencies into their related channel.
 */

import {ChannelProperties} from './diagnostics_types.js';

/**
 * Map of keyed on center frequency with values of related channel for 5GHz.
 * Not all channels are used in all countries. Channels which are not used in
 * any country have been excluded.
 * @type {Map<number, ChannelProperties>}
 */
const FIVE_GHZ_CHANNELS = new Map([
  [5160, {channel: 32}],  [5170, {channel: 34}],  [5180, {channel: 36}],
  [5190, {channel: 38}],  [5200, {channel: 40}],  [5210, {channel: 42}],
  [5220, {channel: 44}],  [5230, {channel: 46}],  [5240, {channel: 48}],
  [5250, {channel: 50}],  [5260, {channel: 52}],  [5270, {channel: 54}],
  [5280, {channel: 56}],  [5290, {channel: 58}],  [5300, {channel: 60}],
  [5310, {channel: 62}],  [5320, {channel: 64}],  [5340, {channel: 68}],
  [5480, {channel: 96}],  [5500, {channel: 100}], [5510, {channel: 102}],
  [5520, {channel: 104}], [5530, {channel: 106}], [5540, {channel: 108}],
  [5550, {channel: 110}], [5560, {channel: 112}], [5570, {channel: 114}],
  [5580, {channel: 116}], [5590, {channel: 118}], [5600, {channel: 120}],
  [5610, {channel: 122}], [5620, {channel: 124}], [5630, {channel: 126}],
  [5640, {channel: 128}], [5660, {channel: 132}], [5670, {channel: 134}],
  [5680, {channel: 136}], [5690, {channel: 138}], [5700, {channel: 140}],
  [5710, {channel: 142}], [5720, {channel: 144}], [5745, {channel: 149}],
  [5755, {channel: 151}], [5765, {channel: 153}], [5775, {channel: 155}],
  [5785, {channel: 157}], [5795, {channel: 159}], [5805, {channel: 161}],
  [5815, {channel: 163}], [5825, {channel: 165}], [5835, {channel: 167}],
  [5845, {channel: 169}], [5855, {channel: 171}], [5865, {channel: 173}],
  [5875, {channel: 175}], [5885, {channel: 177}], [5900, {channel: 180}],
  [5910, {channel: 182}], [5915, {channel: 183}], [5920, {channel: 184}],
  [5935, {channel: 187}], [5940, {channel: 188}], [5945, {channel: 189}],
  [5960, {channel: 192}], [5980, {channel: 196}],
]);

/**
 * Determines 5GHz channel based on |frequency| which is expected to match the
 * channel center frequency.  In all other cases return null.
 * @param {!number} frequency
 * @return {?number} channel
 */
export function convertFrequencyToFiveGhzChannel(frequency) {
  /** @type {ChannelProperties|undefined} */
  const channelProperties = FIVE_GHZ_CHANNELS.get(frequency);
  return channelProperties ? channelProperties.channel : null;
}

/**
 * Converts a MHz frequency into channel number. Should the frequency requested
 * not fall into the algorithm range null is returned.
 * @param {number} frequency Given in MHz.
 * @return {?number} channel
 */
export function convertFrequencyToChannel(frequency) {
  // Handle 2.4GHz channel calculation for channel 1-13.
  if (frequency >= 2412 && frequency <= 2483) {
    return Math.ceil(1 + ((frequency - 2412) / 5));
  }
  // Handle 2.4GHz channel 14 which is a special case for Japan.
  if (frequency >= 2484 && frequency <= 2495) {
    return 14;
  }

  // Return matching 5GHz channel or null.
  return convertFrequencyToFiveGhzChannel(frequency);
}
