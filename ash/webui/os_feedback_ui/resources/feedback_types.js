// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

import '//resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import '//resources/mojo/url/mojom/url.mojom-lite.js';
import '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import '//resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './mojom/os_feedback_ui.mojom-lite.js';

// TODO(xiangdongkong): Remove the following two functions once they have been
// added to ash/common.
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

/**
 * Type alias for HelpContentType.
 * @typedef {ash.osFeedbackUi.mojom.HelpContentType}
 */
export const HelpContentType = ash.osFeedbackUi.mojom.HelpContentType;

/**
 * Type alias for HelpContent.
 * @typedef {ash.osFeedbackUi.mojom.HelpContent}
 */
export const HelpContent = ash.osFeedbackUi.mojom.HelpContent;

/**
 * Type alias for an array of HelpContent.
 * @typedef !Array<!HelpContent>
 */
export let HelpContentList;

/**
 * Type alias for SearchRequest.
 * @typedef {ash.osFeedbackUi.mojom.SearchRequest}
 */
export const SearchRequest = ash.osFeedbackUi.mojom.SearchRequest;

/**
 * Type alias for SearchResponse.
 * @typedef {ash.osFeedbackUi.mojom.SearchResponse}
 */
export const SearchResponse = ash.osFeedbackUi.mojom.SearchResponse;

/**
 * Type alias for the HelpContentProviderInterface.
 * @typedef {ash.osFeedbackUi.mojom.HelpContentProviderInterface}
 */
export const HelpContentProviderInterface =
    ash.osFeedbackUi.mojom.HelpContentProviderInterface;
