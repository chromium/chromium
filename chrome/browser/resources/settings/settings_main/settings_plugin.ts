// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SearchResult} from '../search_settings.js';

// A Settings "plugin" is a top-level Settings UI surface, meaning that it
// corresponds to an entry in the navigation menu. All Settings top-level
// surfaces are implemented as "plugins" and are expected to implement the
// SettingsPlugin interface. <settings-main> is responsible for displaying the
// plugin that corresponds to current route.
//
// Plugins can contain several "views" and can range from very simple (just a
// single "view") to very complex (multiple "views" with parent/child
// relationship from the user's perspective). It is the responsibility of a
// plugin to display the correct view based on the current route, by inheriting
// from the RouteObserverMixin.

export interface SettingsPlugin {
  // Called by <settings-main> when the user initiates a search. The plugin will
  // be included or excluded from the search results depending on the returned
  // SearchResult object. It is the responsibility of a plugin to invoke search
  // and add any highlights before resolving the returned Promise.
  searchContents(query: string): Promise<SearchResult>;
}
