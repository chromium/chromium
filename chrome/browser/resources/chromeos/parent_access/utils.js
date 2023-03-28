// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

/**
 * @param {!String16} str
 * @return {string}
 */
export function decodeMojoString16(str) {
  return str.data.map((ch) => String.fromCodePoint(ch)).join('');
}

/**
 * @param {!Array<number>} pngBytes
 * @return {string}
 */
export function getBase64EncodedSrcForPng(pngBytes) {
  const image = btoa(String.fromCharCode(...pngBytes));
  return 'data:image/png;base64,' + image;
}
