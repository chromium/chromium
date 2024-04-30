// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from 'chrome://resources/js/assert.js';

export interface UrlListEntry {
  title: string;
  url: string;
  imageUrl: string;
}

export function getAbbreviatedUrl(urlString: string) {
  const url = new URL(urlString);
  // Chrome URLs should all have been filtered out.
  assert(url.protocol !== 'chrome:');
  return url.hostname;
}
