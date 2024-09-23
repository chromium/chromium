// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/search/scoring.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

namespace ui {
class ImageModel;
}

// ChromeSearchResult consists of an icon, title text and details text. Title
// and details text can have tagged ranges that are displayed differently from
// default style.
//
// TODO(crbug/1256949): This class now contains many fields we don't use
// anymore, and some outdated terminology and comments. We should update it.
class ChromeSearchResult {
 public:
  using ResultType = ash::AppListSearchResultType;
  using Category = ash::AppListSearchResultCategory;
  using DisplayType = ash::SearchResultDisplayType;
  using MetricsType = ash::SearchResultType;
  using Tag = ash::SearchResultTag;
  using Tags = ash::SearchResultTags;
  using Action = ash::SearchResultAction;
  using Actions = ash::SearchResultActions;
  using IconInfo = ash::SearchResultIconInfo;
  using IconShape = ash::SearchResultIconShape;
  using TextItem = ash::SearchResultTextItem;
  using TextVector = std::vector<TextItem>;
  using TextType = ash::SearchResultTextItemType;

  using MetadataLoaderCallback =
      ash::FileMetadataLoader::MetadataLoaderCallback;

  ChromeSearchResult();

  ChromeSearchResult(const ChromeSearchResult&) = delete;
  ChromeSearchResult& operator=(const ChromeSearchResult&) = delete;

  virtual ~ChromeSearchResult();

  // TODO(crbug.com/1258415): the |title| and |details| related methods should
  // be replaced with convenience wrapper around |title_vector| and
  // |details_vector|.
  const std::u16string& title() const { return metadata_->title; }
  const Tags& title_tags() const { return metadata_->title_tags; }
  const std::u16string& details() const { return metadata_->details; }
  const Tags& details_tags() const { return metadata_->details_tags; }

  const TextVector& title_text_vector() const {
    return metadata_->title_vector;
  }
  bool multiline_title() const { return metadata_->multiline_title; }
  const TextVector& details_text_vector() const {
    return metadata_->details_vector;
  }
  bool multiline_details() const { return metadata_->multiline_details; }
  const TextVector& big_title_text_vector() const {
    return metadata_->big_title_vector;
  }
  const TextVector& big_title_superscript_text_vector() const {
    return metadata_->big_title_superscript_vector;
  }
  const TextVector& keyboard_shortcut_text_vector() const {
    return metadata_->keyboard_shortcut_vector;
  }

  const std::u16string& accessible_name() const {
    return metadata_->accessible_name;
  }
  float rating() const { return metadata_->rating; }
  const std::u16string& formatted_price() const {
    return metadata_->formatted_price;
  }
  const std::string& id() const { return metadata_->id; }
  Category category() const { return metadata_->category; }
  bool best_match() const { return metadata_->best_match; }
  DisplayType display_type() const { return metadata_->display_type; }
  ash::AppListSearchResultType result_type() const {
    return metadata_->result_type;
  }
  MetricsType metrics_type() const { return metadata_->metrics_type; }
  const std::optional<ash::ContinueFileSuggestionType>&
  continue_file_suggestion_type() const {
    return metadata_->continue_file_suggestion_type;
  }
  const Actions& actions() const { return metadata_->actions; }
  double display_score() const { return metadata_->display_score; }
  bool is_recommendation() const { return metadata_->is_recommendation; }
  bool skip_update_animation() const {
    return metadata_->skip_update_animation;
  }
  const IconInfo& icon() const { return metadata_->icon; }
  const gfx::ImageSkia& chip_icon() const { return metadata_->chip_icon; }
  const ui::ImageModel& badge_icon() const { return metadata_->badge_icon; }
  const std::optional<ash::SystemInfoAnswerCardData>
  system_info_answer_card_data() const {
    return metadata_->system_info_answer_card_data;
  }
  // Only file results have set the filepath.
  const base::FilePath& filePath() const { return metadata_->file_path; }
  const base::FilePath& displayable_file_path() const {
    return metadata_->displayable_file_path;
  }
  ash::FileMetadataLoader* file_metadata_loader() {
    return &metadata_->file_metadata_loader;
  }

  // The following methods set Chrome side data here, and call model updater
  // interface to update Ash.
  void SetTitle(const std::u16string& title);
  void SetTitleTags(const Tags& tags);
  void MaybeUpdateTitleVector();
  void SetDetails(const std::u16string& details);
  void SetDetailsTags(const Tags& tags);
  void MaybeUpdateDetailsVector();
  void SetTitleTextVector(const TextVector& text_vector);
  void SetMultilineTitle(bool multiline_title);
  void SetDetailsTextVector(const TextVector& text_vector);
  void SetMultilineDetails(bool multiline_details);
  void SetBigTitleTextVector(const TextVector& text_vector);
  void SetBigTitleSuperscriptTextVector(const TextVector& text_vector);
  void SetKeyboardShortcutTextVector(const TextVector& text_vector);
  void SetAccessibleName(const std::u16string& name);
  void SetRating(float rating);
  void SetFormattedPrice(const std::u16string& formatted_price);
  void SetCategory(Category category);
  void SetBestMatch(bool best_match);
  void SetDisplayType(DisplayType display_type);
  void SetResultType(ResultType result_type);
  void SetMetricsType(MetricsType metrics_type);
  void SetContinueFileSuggestionType(
      ash::ContinueFileSuggestionType continue_file_suggestion_type);
  void SetDisplayScore(double display_score);
  void SetActions(const Actions& actions);
  void SetIsRecommendation(bool is_recommendation);
  void SetSkipUpdateAnimation(bool skip_update_animation);
  void SetIcon(const IconInfo& icon);
  void SetIconDimension(const int dimension);
  void SetChipIcon(const gfx::ImageSkia& icon);
  void SetBadgeIcon(const ui::ImageModel& badge_icon);
  void SetUseBadgeIconBackground(bool use_badge_icon_background);
  void SetSystemInfoAnswerCardData(
      ash::SystemInfoAnswerCardData answer_card_info);
  void SetFilePath(base::FilePath file_path);
  void SetDisplayableFilePath(base::FilePath displayable_file_path);
  void SetMetadataLoaderCallback(MetadataLoaderCallback callback);

  void SetSearchResultMetadata();

  void SetMetadata(std::unique_ptr<ash::SearchResultMetadata> metadata) {
    metadata_ = std::move(metadata);
  }
  std::unique_ptr<ash::SearchResultMetadata> CloneMetadata() const {
    return std::make_unique<ash::SearchResultMetadata>(*metadata_);
  }

  void set_model_updater(AppListModelUpdater* model_updater) {
    model_updater_ = model_updater;
  }
  AppListModelUpdater* model_updater() const { return model_updater_; }

  double relevance() const { return relevance_; }
  void set_relevance(double relevance) { relevance_ = relevance; }

  crosapi::mojom::SearchResult::AnswerType answer_type() const {
    return answer_type_;
  }
  void set_answer_type(crosapi::mojom::SearchResult::AnswerType answer_type) {
    answer_type_ = answer_type;
  }

  app_list::Scoring& scoring() { return scoring_; }

  const app_list::Scoring& scoring() const { return scoring_; }

  const base::flat_map<std::string, double>& ranker_scores() const {
    return ranker_scores_;
  }
  void set_ranker_score(std::string ranking_method, double score) {
    ranker_scores_[ranking_method] = score;
  }

  bool dismiss_view_on_open() const { return dismiss_view_on_open_; }
  void set_dismiss_view_on_open(bool dismiss_view_on_open) {
    dismiss_view_on_open_ = dismiss_view_on_open;
  }

  // Maybe returns a Drive file ID for this result, if applicable.
  virtual std::optional<std::string> DriveId() const;

  // Maybe returns a url for this result, if applicable.
  virtual std::optional<GURL> url() const;

  // Invokes a custom action on the result. It does nothing by default.
  virtual void InvokeAction(ash::SearchResultActionType action);

  // Opens the result. Clients should use AppListViewDelegate::OpenSearchResult.
  virtual void Open(int event_flags) = 0;

  // Called if set visible/hidden.
  virtual void OnVisibilityChanged(bool visibility);

  base::WeakPtr<ChromeSearchResult> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  // These id setters should be called in derived class constructors only.
  void set_id(const std::string& id) { metadata_->id = id; }

 private:
  // The relevance of this result, as decided by the search provider that
  // created it. This shouldn't be modified by ranking, and is not used by ash.
  double relevance_ = 0;

  // Components of this result's scoring, as decided by the Rankers that mix
  // together search results. May be updated several times over the lifetime
  // of a result. Its |FinalScore| should be used to set the display score,
  // which may be used directly by ash.
  //
  // Only used when the categorical search flag is enabled.
  app_list::Scoring scoring_;

  // This field specifies the omnibox answer card type.
  crosapi::mojom::SearchResult::AnswerType answer_type_;

  // Relevance scores keyed by a string describing the ranking method it was
  // obtained from. These can include scores from intermediate ranking steps, as
  // well as prototype scores to be inspected for experimentation.
  //
  // TODO(crbug.com/1292783): Move this to a map<string, string> of debug info
  // that contains more than just scores, and display the contents of
  // |scoring_| in chrome://launcher-internals
  base::flat_map<std::string, double> ranker_scores_;

  // More often than not, calling Open() on a ChromeSearchResult will cause the
  // app list view to be closed as a side effect. Because opening apps can take
  // some time, the app list view is eagerly dismissed by default after invoking
  // Open() for added polish. Some ChromeSearchResults may not appreciate this
  // behavior so it can be disabled as needed.
  bool dismiss_view_on_open_ = true;

  // Whether the text vector is explicitly set by chrome.
  bool explicit_title_vector_ = false;
  bool explicit_details_vector_ = false;

  std::unique_ptr<ash::SearchResultMetadata> metadata_;

  raw_ptr<AppListModelUpdater> model_updater_ = nullptr;

  base::WeakPtrFactory<ChromeSearchResult> weak_ptr_factory_{this};
};

::std::ostream& operator<<(::std::ostream& os,
                           const ChromeSearchResult& result);

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
