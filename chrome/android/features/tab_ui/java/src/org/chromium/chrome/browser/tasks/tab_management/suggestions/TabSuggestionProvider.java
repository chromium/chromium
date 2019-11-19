// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import java.util.List;

/**
 * Defines the contract between {@link TabSuggestionsFetcher} and all the client-side suggestion
 * providers.
 */
public interface TabSuggestionProvider { List<TabSuggestion> suggest(TabContext tabContext); }
