// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/model/app_list_model_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/unguessable_token.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"

namespace ash {

class SearchResultObserver;

// SearchResult consists of an icon, title text and details text. Title and
// details text can have tagged ranges that are displayed differently from
// default style.
class APP_LIST_MODEL_EXPORT SearchResult {
 public:
  using ResultType = ash::AppListSearchResultType;
  using DisplayType = ash::SearchResultDisplayType;
  using Tag = ash::SearchResultTag;
  using Tags = ash::SearchResultTags;
  using Action = ash::SearchResultAction;
  using Actions = ash::SearchResultActions;
  using DisplayLocation = ash::SearchResultDisplayLocation;
  using DisplayIndex = ash::SearchResultDisplayIndex;

  SearchResult();
  virtual ~SearchResult();

  const gfx::ImageSkia& icon() const { return metadata_->icon; }
  void SetIcon(const gfx::ImageSkia& icon);

  const gfx::ImageSkia& chip_icon() const { return metadata_->chip_icon; }
  void SetChipIcon(const gfx::ImageSkia& chip_icon);

  const gfx::ImageSkia& badge_icon() const { return metadata_->badge_icon; }
  void SetBadgeIcon(const gfx::ImageSkia& badge_icon);

  const base::string16& title() const { return metadata_->title; }
  void set_title(const base::string16& title);

  const Tags& title_tags() const { return metadata_->title_tags; }
  void set_title_tags(const Tags& tags) { metadata_->title_tags = tags; }

  const base::string16& details() const { return metadata_->details; }
  void set_details(const base::string16& details) {
    metadata_->details = details;
  }

  const Tags& details_tags() const { return metadata_->details_tags; }
  void set_details_tags(const Tags& tags) { metadata_->details_tags = tags; }

  const base::string16& accessible_name() const {
    return metadata_->accessible_name;
  }
  void set_accessible_name(const base::string16& name) {
    metadata_->accessible_name = name;
  }

  float rating() const { return metadata_->rating; }
  void SetRating(float rating);

  const base::string16& formatted_price() const {
    return metadata_->formatted_price;
  }
  void SetFormattedPrice(const base::string16& formatted_price);

  const base::Optional<GURL>& query_url() const { return metadata_->query_url; }
  void set_query_url(const GURL& url) { metadata_->query_url = url; }

  const base::Optional<std::string>& equivalent_result_id() const {
    return metadata_->equivalent_result_id;
  }
  void set_equivalent_result_id(const std::string& equivalent_result_id) {
    metadata_->equivalent_result_id = equivalent_result_id;
  }

  const std::string& id() const { return metadata_->id; }

  double display_score() const { return metadata_->display_score; }
  void set_display_score(double display_score) {
    metadata_->display_score = display_score;
  }

  DisplayType display_type() const { return metadata_->display_type; }
  void set_display_type(DisplayType display_type) {
    metadata_->display_type = display_type;
  }

  ResultType result_type() const { return metadata_->result_type; }
  void set_result_type(ResultType result_type) {
    metadata_->result_type = result_type;
  }

  DisplayLocation display_location() const {
    return metadata_->display_location;
  }
  void set_display_location(DisplayLocation display_location) {
    metadata_->display_location = display_location;
  }

  DisplayIndex display_index() const { return metadata_->display_index; }
  void set_display_index(DisplayIndex display_index) {
    metadata_->display_index = display_index;
  }

  float position_priority() const { return metadata_->position_priority; }
  void set_position_priority(float position_priority) {
    metadata_->position_priority = position_priority;
  }

  int result_subtype() const { return metadata_->result_subtype; }
  void set_result_subtype(int result_subtype) {
    metadata_->result_subtype = result_subtype;
  }

  int distance_from_origin() { return distance_from_origin_; }
  void set_distance_from_origin(int distance) {
    distance_from_origin_ = distance;
  }

  const Actions& actions() const { return metadata_->actions; }
  void SetActions(const Actions& sets);

  bool is_installing() const { return is_installing_; }
  void SetIsInstalling(bool is_installing);

  int percent_downloaded() const { return percent_downloaded_; }
  void SetPercentDownloaded(int percent_downloaded);

  bool notify_visibility_change() const {
    return metadata_->notify_visibility_change;
  }

  void set_notify_visibility_change(bool notify_visibility_change) {
    metadata_->notify_visibility_change = notify_visibility_change;
  }

  bool is_omnibox_search() const { return metadata_->is_omnibox_search; }
  void set_is_omnibox_search(bool is_omnibox_search) {
    metadata_->is_omnibox_search = is_omnibox_search;
  }

  bool is_visible() const { return is_visible_; }
  void set_is_visible(bool is_visible) { is_visible_ = is_visible; }

  void NotifyItemInstalled();

  void AddObserver(SearchResultObserver* observer);
  void RemoveObserver(SearchResultObserver* observer);

  // Invokes a custom action on the result. It does nothing by default.
  virtual void InvokeAction(int action_index, int event_flags);

  void SetMetadata(std::unique_ptr<ash::SearchResultMetadata> metadata);
  std::unique_ptr<ash::SearchResultMetadata> TakeMetadata() {
    return std::move(metadata_);
  }
  std::unique_ptr<ash::SearchResultMetadata> CloneMetadata() const {
    return std::make_unique<ash::SearchResultMetadata>(*metadata_);
  }

 protected:
  void set_id(const std::string& id) { metadata_->id = id; }

 private:
  friend class SearchController;

  // Opens the result. Clients should use AppListViewDelegate::OpenSearchResult.
  virtual void Open(int event_flags);

  // The Manhattan distance from the origin of all search results to this
  // result. This is logged for UMA.
  int distance_from_origin_ = -1;

  bool is_installing_ = false;
  int percent_downloaded_ = 0;
  bool is_visible_ = true;

  std::unique_ptr<ash::SearchResultMetadata> metadata_;

  base::ObserverList<SearchResultObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchResult);
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_
