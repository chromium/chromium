// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 *
 * TODO(xiangdongkong): When the fake API is replaced by mojo these can be
 * re-aliased to the corresponding mojo types, or replaced by them.
 */

/**
 * Enum for help content types.
 * @enum {string}
 */
export const HelpContentType = {
  ARTICLE: 'article',
  FORUM: 'forum'
};

/**
 * Type alias for HelpContent.
 * @typedef {{
 *   title: string,
 *   url: string,
 *   content_type: HelpContentType
 * }}
 */
export let HelpContent;

/**
 * Type alias for an array of HelpContent.
 * @typedef !Array<!HelpContent>
 */
export let HelpContentList;

/**
 * Type alias for the HelpContentProviderInterface.
 * TODO(xiangdongkong): Replace with a real mojo type when implemented.
 * @typedef {{
 *   getHelpContents: !function(string, number): !Promise<HelpContentList>,
 * }}
 */
export let HelpContentProviderInterface;
