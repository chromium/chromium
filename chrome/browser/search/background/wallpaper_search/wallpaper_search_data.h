// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/token.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search.mojom.h"

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_DATA_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_DATA_H_

// Represents a history entry in wallpaper search.
struct HistoryEntry {
  HistoryEntry();
  explicit HistoryEntry(const base::Token& id);
  HistoryEntry(const HistoryEntry&);
  HistoryEntry(HistoryEntry&&);
  ~HistoryEntry();

  HistoryEntry& operator=(const HistoryEntry&);
  HistoryEntry& operator=(HistoryEntry&&);
  bool operator==(const HistoryEntry& rhs) const;

  base::Token id;
  std::optional<std::string> subject;
  std::optional<std::string> style;
  std::optional<std::string> mood;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_DATA_H_
