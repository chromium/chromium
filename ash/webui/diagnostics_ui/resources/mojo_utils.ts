// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';


/**
 * Converts a JS string to mojo_base::mojom::String16 object.
 */
export function stringToMojoString16(str: string): String16 {
  const arr: number[] = [];
  for (let i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }
  return {data: arr};
}

/**
 * Converts mojo_base::mojom::String16 to a JS string.
 */
export function mojoString16ToString(str16: String16): string {
  return str16.data.map((ch: number) => String.fromCodePoint(ch)).join('');
}
