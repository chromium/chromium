// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

/**
 * @fileoverview This file provides shared utility functions used by the custom
 * elements in the Memories landing page.
 */

/**
 * @param {!Url} url
 * @return {string} The domain name of the URL without the leading 'www.'.
 */
export function getHostnameFromUrl(url) {
  return new URL(url.url).hostname.replace(/^(www\.)/, '');
}
