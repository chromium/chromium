// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab_group_shared_vars.js';

import {Color} from './tab_group_types.mojom-webui.js';

const colorMap = new Map<Color, string>([
  [Color.kGrey, 'grey'],
  [Color.kBlue, 'blue'],
  [Color.kRed, 'red'],
  [Color.kYellow, 'yellow'],
  [Color.kGreen, 'green'],
  [Color.kPink, 'pink'],
  [Color.kPurple, 'purple'],
  [Color.kCyan, 'cyan'],
  [Color.kOrange, 'orange'],
]);

export function colorName(color: Color): string {
  if (!colorMap.has(color)) {
    throw Error('Undefined color id');
  }

  return colorMap.get(color)!;
}
