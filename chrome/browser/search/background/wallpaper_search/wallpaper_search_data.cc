// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_data.h"

#include "base/token.h"

HistoryEntry::HistoryEntry() = default;
HistoryEntry::HistoryEntry(const base::Token& id) : id(id) {}
HistoryEntry::HistoryEntry(const HistoryEntry&) = default;
HistoryEntry::HistoryEntry(HistoryEntry&&) = default;
HistoryEntry::~HistoryEntry() = default;

HistoryEntry& HistoryEntry::operator=(const HistoryEntry&) = default;
HistoryEntry& HistoryEntry::operator=(HistoryEntry&&) = default;
bool HistoryEntry::operator==(const HistoryEntry& rhs) const {
  return this->id.ToString() == rhs.id.ToString() &&
         this->subject == rhs.subject && this->style == rhs.style &&
         this->mood == rhs.mood;
}
