// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type Vec4 = [number, number, number, number];

const rgbRe =
    /rgb[a]?\(\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,?(\s*[0-9.e-]*)?\s*\)/i;
const hexRe = /#([0-9a-f]{8,8})|([0-9a-f]{6,6})|([0-9a-f]{3,3})/i;
const cache: {[index: string]: Vec4} = {};

function normalize(channels: Vec4, maxValue: number = 255): Vec4 {
  return [
    channels[0] / maxValue,
    channels[1] / maxValue,
    channels[2] / maxValue,
    channels[3] / maxValue,
  ];
}

/**
 * Parses a CSS color string.
 * Only supports rgb(...), rgba(...) and hex values.
 * Alpha values are dropped from rgba(...).
 */
export function parseCssColor(color: string): Vec4 {
  if (cache[color] == null) {
    let match = color.match(rgbRe);
    let parsedColor: Vec4 = [255, 255, 255, 255];

    if (match != null) {
      const [r, g, b, a] =
          match.slice(1, 5).map((value: string, index: number) => {
            let parsedValue = Math.floor(Number(value));
            if (index === 3) {
              parsedValue = parsedValue * 255;
            }
            return Number.isNaN(parsedValue) ? 255 : parsedValue;
          });
      parsedColor = [
        r != null ? r : 255,
        g != null ? g : 255,
        b != null ? b : 255,
        a != null ? a : 255,
      ];
    } else {
      match = color.match(hexRe);

      if (match != null) {
        const hexString = match[1] || match[2] || match[3];
        const channelSize = hexString.length < 6 ? 1 : 2;
        const channels: number[] = [];

        for (let i = 0; i < hexString.length; i += channelSize) {
          const numberString = hexString.slice(i, i + channelSize);
          if (!/^[a-fA-F0-9]+$/.test(numberString)) {
            throw new Error('NaN');
          }
          // Needed to parse hexadecimal.
          // tslint:disable-next-line:ban
          let channel = parseInt(numberString, 16);
          if (channelSize === 1) {
            channel = (channel << 4) + channel;
          }
          if (Number.isNaN(channel)) {
            channel = 255;
          }
          channels.push(channel);
        }

        while (channels.length < 4) {
          channels.push(255);
        }

        parsedColor = channels as Vec4;
      }
    }

    cache[color] = normalize(parsedColor);
  }

  return cache[color].slice() as Vec4;
}
