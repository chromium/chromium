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
#include "ash/public/interfaces/app_list.mojom.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/unguessable_token.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"

namespace app_list {

class SearchResultObserver;

// SearchResult consists of an icon, title text and details text. Title and
// details text can have tagged ranges that are displayed differently from
// default style.
class APP_LIST_MODEL_EXPORT SearchResult {
 public:
  using ResultType = ash::SearchResultType;
  using DisplayType = ash::SearchResultDisplayType;
  using Tag = ash::SearchResultTag;
  using Tags = ash::SearchResultTags;
  using Action = ash::SearchResultAction;
  using Actions = ash::SearchResultActions;

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

  const base::Optional<base::UnguessableToken>& answer_card_contents_token()
      const {
    return metadata_->answer_card_contents_token;
  }
  void set_answer_card_contents_token(
      const base::Optional<base::UnguessableToken>& token) {
    metadata_->answer_card_contents_token = token;
  }

  gfx::Size answer_card_size() const {
    return metadata_->answer_card_size.value_or(gfx::Size());
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

  bool is_omnibox_search() const { return metadata_->is_omnibox_search; }
  void set_is_omnibox_search(bool is_omnibox_search) {
    metadata_->is_omnibox_search = is_omnibox_search;
  }

  void NotifyItemInstalled();

  void AddObserver(SearchResultObserver* observer);
  void RemoveObserver(SearchResultObserver* observer);

  // Invokes a custom action on the result. It does nothing by default.
  virtual void InvokeAction(int action_index, int event_flags);

  void SetMetadata(ash::mojom::SearchResultMetadataPtr metadata);
  ash::mojom::SearchResultMetadataPtr CloneMetadata() const {
    return metadata_.Clone();
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

  ash::mojom::SearchResultMetadataPtr metadata_;

  base::ObserverList<SearchResultObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchResult);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_H_
