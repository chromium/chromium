// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HelpContent} from './os_feedback_ui.mojom-webui.js';

/**
 * @fileoverview
 * Type aliases for the app.
 */

/** Type alias for an array of HelpContent.*/
export type HelpContentList = HelpContent[];
export const HelpContentList = Array<HelpContent>;

/**
 * Type alias for search result. When isPopularContent is true, the contentList
 * contains top popular help contents, i.e. returned where the search query is
 * empty. The isQueryEmpty is true when the current query is empty. The
 * isPopularContent is true when the current query is not empty and no matches
 * are found.
 */
export interface SearchResult {
  contentList: HelpContentList;
  isQueryEmpty: boolean;
  isPopularContent: boolean;
}
