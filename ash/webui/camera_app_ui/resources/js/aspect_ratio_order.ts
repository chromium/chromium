// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from './assert.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {convertMojoToAspectRatio} from './mojo/type_utils.js';
import {AspectRatioSet} from './type.js';

let aspectRatioOrder: AspectRatioSet[]|null = null;

/**
 *  Setup `aspectRatioOrder` from private API.
 */
export async function setup(): Promise<void> {
  const mojoAspectRatioOrder =
      await ChromeHelper.getInstance().getAspectRatioOrder();
  aspectRatioOrder = mojoAspectRatioOrder.map(
      (mojoAspectRatio) => convertMojoToAspectRatio(mojoAspectRatio));
}

/**
 *  Get the aspect ratio order. `setup` must be done before calling this
 *  function.
 */
export function getAspectRatioOrder(): AspectRatioSet[] {
  return assertExists(aspectRatioOrder);
}
