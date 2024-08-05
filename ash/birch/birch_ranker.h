// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_RANKER_H_
#define ASH_BIRCH_BIRCH_RANKER_H_

#include "ash/ash_export.h"
#include "ash/birch/birch_item.h"
#include "base/time/time.h"

namespace ash {

class BirchAttachmentItem;
class BirchCalendarItem;
class BirchCoralItem;
class BirchFileItem;
class BirchTabItem;
class BirchWeatherItem;
class BirchLostMediaItem;

// Computes a ranking for birch items of various types. The ranking depends on
// the time of day. The "now" time is a constructor parameter to allow testing.
// The ranking numbers are the row numbers from the "Triggers & Priority"
// spreadsheet, http://go/birch-triggers
class ASH_EXPORT BirchRanker {
 public:
  explicit BirchRanker(base::Time now);
  BirchRanker(const BirchRanker&) = delete;
  BirchRanker& operator=(const BirchRanker&) = delete;
  ~BirchRanker();

  // Ranks BirchItems of the appropriate time. The items are mutated in place
  // to have their ranking field updated. This avoids copying the items.
  void RankCalendarItems(std::vector<BirchCalendarItem>* items);
  void RankAttachmentItems(std::vector<BirchAttachmentItem>* items);
  void RankFileSuggestItems(std::vector<BirchFileItem>* items);
  void RankRecentTabItems(std::vector<BirchTabItem>* items);
  void RankLastActiveItems(std::vector<BirchLastActiveItem>* items);
  void RankMostVisitedItems(std::vector<BirchMostVisitedItem>* items);
  void RankSelfShareItems(std::vector<BirchSelfShareItem>* items);
  void RankLostMediaItems(std::vector<BirchLostMediaItem>* items);
  void RankWeatherItems(std::vector<BirchWeatherItem>* items);
  void RankReleaseNotesItems(std::vector<BirchReleaseNotesItem>* items);
  void RankCoralItems(std::vector<BirchCoralItem>* items);

  // Returns whether `now_` is before noon today. Public for testing.
  bool IsMorning() const;

  // Returns whether `now_` is after 5pm today. Public for testing.
  bool IsEvening() const;

 private:
  // Returns whether `now_` is in the middle of a calendar event's times.
  bool IsOngoingEvent(const BirchCalendarItem& item) const;

  // Returns whether `item` is scheduled tomorrow (after midnight tonight).
  bool IsTomorrowEvent(const BirchCalendarItem& item) const;

  float GetReleaseNotesItemRanking(const BirchReleaseNotesItem& item) const;

  base::Time now_;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_RANKER_H_
