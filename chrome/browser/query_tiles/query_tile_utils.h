// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_QUERY_TILES_QUERY_TILE_UTILS_H_
#define CHROME_BROWSER_QUERY_TILES_QUERY_TILE_UTILS_H_

namespace query_tiles {
// Check whether query tile is enabled.
bool IsQueryTilesEnabled();

// Return the coutry code used for getting the tiles.
std::string GetCountryCode();
}  // namespace query_tiles

#endif  // CHROME_BROWSER_QUERY_TILES_QUERY_TILE_UTILS_H_
