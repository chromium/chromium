// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */


/**
 * Type alias for an array of HelpContent.
 * typedef !Array<!HelpContent>
 */
export let HelpContentList;

/**
 * Type alias for search result. When isPopularContent is true, the contentList
 * contains top popular help contents, i.e. returned where the search query is
 * empty. The isQueryEmpty is true when the current query is empty. The
 * isPopularContent is true when the current query is not empty and no matches
 * are found.
 * typedef {{
 *   contentList: HelpContentList,
 *   isQueryEmpty: boolean,
 *   isPopularContent: boolean
 * }}
 */
export let SearchResult;
