// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import '//resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';

/**
 * Converts a JS string to mojo_base::mojom::String16 object.
 * @param {string} str
 * @return {!mojoBase.mojom.String16}
 */
export function stringToMojoString16(str) {
  const arr = [];
  for (let i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }
  return {data: arr};
}

/**
 * Converts mojo_base::mojom::String16 to a JS string.
 * @param {!mojoBase.mojom.String16} str16
 * @return {string}
 */
export function mojoString16ToString(str16) {
  return str16.data.map(ch => String.fromCodePoint(ch)).join('');
}
