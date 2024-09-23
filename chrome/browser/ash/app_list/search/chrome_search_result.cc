// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

#include <map>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/app_list/app_context_menu.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

ChromeSearchResult::ChromeSearchResult()
    : metadata_(std::make_unique<ash::SearchResultMetadata>()) {}

ChromeSearchResult::~ChromeSearchResult() = default;

void ChromeSearchResult::SetActions(const Actions& actions) {
  metadata_->actions = actions;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDisplayScore(double display_score) {
  metadata_->display_score = display_score;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetTitle(const std::u16string& title) {
  metadata_->title = title;
  MaybeUpdateTitleVector();
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetTitleTags(const Tags& tags) {
  metadata_->title_tags = tags;
  MaybeUpdateTitleVector();
  SetSearchResultMetadata();
}

void ChromeSearchResult::MaybeUpdateTitleVector() {
  // Create and setup title tags if not set explicitly.
  if (!explicit_title_vector_) {
    std::vector<TextItem> text_vector;
    TextItem text_item(ash::SearchResultTextItemType::kString);
    text_item.SetText(metadata_->title);
    text_item.SetTextTags(metadata_->title_tags);
    text_vector.push_back(text_item);
    metadata_->title_vector = text_vector;
  }
}

void ChromeSearchResult::SetDetails(const std::u16string& details) {
  metadata_->details = details;
  MaybeUpdateDetailsVector();
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDetailsTags(const Tags& tags) {
  metadata_->details_tags = tags;
  MaybeUpdateDetailsVector();
  SetSearchResultMetadata();
}

void ChromeSearchResult::MaybeUpdateDetailsVector() {
  // Create and setup details tags if not set explicitly.
  if (!explicit_details_vector_) {
    std::vector<TextItem> text_vector;
    TextItem text_item(ash::SearchResultTextItemType::kString);
    text_item.SetText(metadata_->details);
    text_item.SetTextTags(metadata_->details_tags);
    text_vector.push_back(text_item);
    metadata_->details_vector = text_vector;
  }
}

void ChromeSearchResult::SetTitleTextVector(const TextVector& text_vector) {
  metadata_->title_vector = text_vector;
  explicit_title_vector_ = true;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetMultilineTitle(bool multiline_title) {
  metadata_->multiline_title = multiline_title;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDetailsTextVector(const TextVector& text_vector) {
  metadata_->details_vector = text_vector;
  explicit_details_vector_ = true;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetMultilineDetails(bool multiline_details) {
  metadata_->multiline_details = multiline_details;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetBigTitleTextVector(const TextVector& text_vector) {
  metadata_->big_title_vector = text_vector;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetBigTitleSuperscriptTextVector(
    const TextVector& text_vector) {
  metadata_->big_title_superscript_vector = text_vector;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetKeyboardShortcutTextVector(
    const TextVector& text_vector) {
  metadata_->keyboard_shortcut_vector = text_vector;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetAccessibleName(const std::u16string& name) {
  metadata_->accessible_name = name;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetRating(float rating) {
  metadata_->rating = rating;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetFormattedPrice(
    const std::u16string& formatted_price) {
  metadata_->formatted_price = formatted_price;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetCategory(Category category) {
  metadata_->category = category;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetBestMatch(bool best_match) {
  metadata_->best_match = best_match;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDisplayType(DisplayType display_type) {
  metadata_->display_type = display_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetResultType(ResultType result_type) {
  metadata_->result_type = result_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetMetricsType(MetricsType metrics_type) {
  metadata_->metrics_type = metrics_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetContinueFileSuggestionType(
    ash::ContinueFileSuggestionType type) {
  metadata_->continue_file_suggestion_type = type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetIsRecommendation(bool is_recommendation) {
  metadata_->is_recommendation = is_recommendation;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetSkipUpdateAnimation(bool skip_update_animation) {
  metadata_->skip_update_animation = skip_update_animation;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetIcon(const IconInfo& icon) {
  TRACE_EVENT0("ui", "ChromeSearchResult::SetIcon");
  metadata_->icon = icon;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetIconDimension(const int dimension) {
  metadata_->icon.dimension = dimension;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetChipIcon(const gfx::ImageSkia& chip_icon) {
  TRACE_EVENT0("ui", "ChromeSearchResult::SetChipIcon");
  chip_icon.EnsureRepsForSupportedScales();
  metadata_->chip_icon = chip_icon;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetBadgeIcon(const ui::ImageModel& badge_icon) {
  metadata_->badge_icon = badge_icon;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetUseBadgeIconBackground(
    bool use_badge_icon_background) {
  metadata_->use_badge_icon_background = use_badge_icon_background;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetSystemInfoAnswerCardData(
    ash::SystemInfoAnswerCardData answer_card_info) {
  metadata_->system_info_answer_card_data = answer_card_info;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetFilePath(base::FilePath file_path) {
  metadata_->file_path = file_path;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDisplayableFilePath(
    base::FilePath displayable_file_path) {
  metadata_->displayable_file_path = std::move(displayable_file_path);
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetMetadataLoaderCallback(
    MetadataLoaderCallback callback) {
  metadata_->file_metadata_loader.SetLoaderCallback(std::move(callback));
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetSearchResultMetadata() {
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

std::optional<std::string> ChromeSearchResult::DriveId() const {
  return std::nullopt;
}

std::optional<GURL> ChromeSearchResult::url() const {
  return std::nullopt;
}

void ChromeSearchResult::InvokeAction(ash::SearchResultActionType action) {}

void ChromeSearchResult::OnVisibilityChanged(bool visibility) {
  VLOG(1) << " Visibility change to " << visibility << " and ID is " << id();
}

::std::ostream& operator<<(::std::ostream& os,
                           const ChromeSearchResult& result) {
  return os << result.id() << " " << result.scoring();
}
