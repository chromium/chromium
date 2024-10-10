// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_result.h"

#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/common/image_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

using CrosApiSearchResult = crosapi::mojom::SearchResult;

namespace app_list {
namespace {

using ::ash::string_matching::FuzzyTokenizedStringMatch;
using ::ash::string_matching::TokenizedString;

// Parameters for FuzzyTokenizedStringMatch.
constexpr bool kUseWeightedRatio = false;

constexpr double kRelevanceThreshold = 0.32;

// Flag to enable/disable diacritics stripping.
constexpr bool kStripDiacritics = true;

// Flag to enable/disable acronym matcher.
constexpr bool kUseAcronymMatcher = true;

// Priority numbers for deduplication. Higher numbers indicate higher priority.
constexpr int kRichEntityPriority = 2;
constexpr int kHistoryPriority = 1;
constexpr int kDefaultPriority = 0;

// crosapi OmniboxType to vector icon, used for app list.
const gfx::VectorIcon& TypeToVectorIcon(CrosApiSearchResult::OmniboxType type) {
  switch (type) {
    case CrosApiSearchResult::OmniboxType::kDomain:
      return ash::kOmniboxGenericIcon;
    case CrosApiSearchResult::OmniboxType::kSearch:
      return ash::kSearchIcon;
    case CrosApiSearchResult::OmniboxType::kHistory:
      return ash::kHistoryIcon;
    default:
      NOTREACHED_IN_MIGRATION();
      return ash::kOmniboxGenericIcon;
  }
}

}  // namespace

OmniboxResult::OmniboxResult(Profile* profile,
                             AppListControllerDelegate* list_controller,
                             crosapi::mojom::SearchResultPtr search_result,
                             const std::u16string& query)
    : consumer_receiver_(this, std::move(search_result->receiver)),
      profile_(profile),
      list_controller_(list_controller),
      search_result_(std::move(search_result)),
      query_(query),
      contents_(search_result_->contents.value_or(u"")),
      description_(search_result_->description.value_or(u"")) {
  SetDisplayType(DisplayType::kList);
  SetResultType(ResultType::kOmnibox);
  SetMetricsType(GetSearchResultType());

  set_id(search_result_->stripped_destination_url->spec());

  // Omnibox results are categorized as Search and Assistant if they are search
  // suggestions, and Web otherwise.
  SetCategory(search_result_->omnibox_type ==
                      CrosApiSearchResult::OmniboxType::kSearch
                  ? Category::kSearchAndAssistant
                  : Category::kWeb);

  if (IsRichEntity()) {
    dedup_priority_ = kRichEntityPriority;
  } else if (search_result_->omnibox_type ==
             CrosApiSearchResult::OmniboxType::kHistory) {
    dedup_priority_ = kHistoryPriority;
  } else {
    dedup_priority_ = kDefaultPriority;
  }

  SetSkipUpdateAnimation(search_result_->metrics_type ==
                         CrosApiSearchResult::MetricsType::kSearchWhatYouTyped);

  UpdateIcon();
  UpdateTitleAndDetails();

  // Requires the title for fuzzy match and thus must be calculated after the
  // title is set.
  UpdateRelevance();

  if (OptionalBoolIsTrue(search_result_->is_omnibox_search)) {
    InitializeButtonActions({ash::SearchResultActionType::kRemove});
  }

  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->AddObserver(this);
}

OmniboxResult::~OmniboxResult() {
  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->RemoveObserver(this);
}

void OmniboxResult::UpdateRelevance() {
  double normalized_autocomplete_relevance =
      search_result_->relevance / kMaxOmniboxScore;

  if (search_features::IsLauncherFuzzyMatchForOmniboxEnabled()) {
    double title_relevance = CalculateTitleRelevance();
    if (title_relevance < kRelevanceThreshold) {
      scoring().set_filtered(true);
    }
  }

  // Derive relevance from autocomplete relevance and normalize it to [0, 1].
  set_relevance(normalized_autocomplete_relevance);
}

double OmniboxResult::CalculateTitleRelevance() const {
  const TokenizedString tokenized_title(title(), TokenizedString::Mode::kWords);
  const TokenizedString tokenized_query(query_,
                                        TokenizedString::Mode::kCamelCase);

  if (tokenized_query.text().empty() || tokenized_title.text().empty()) {
    static constexpr double kDefaultRelevance = 0.0;
    return kDefaultRelevance;
  }

  FuzzyTokenizedStringMatch match;
  return match.Relevance(tokenized_query, tokenized_title, kUseWeightedRatio,
                         kStripDiacritics, kUseAcronymMatcher);
}

std::optional<GURL> OmniboxResult::url() const {
  return search_result_->destination_url;
}

void OmniboxResult::Open(int event_flags) {
  list_controller_->OpenURL(
      profile_, *search_result_->destination_url,
      PageTransitionToUiPageTransition(search_result_->page_transition),
      ui::DispositionFromEventFlags(event_flags));
}

ash::SearchResultType OmniboxResult::GetSearchResultType() const {
  switch (search_result_->metrics_type) {
    case CrosApiSearchResult::MetricsType::kWhatYouTyped:
      return ash::OMNIBOX_URL_WHAT_YOU_TYPED;
    case CrosApiSearchResult::MetricsType::kRecentlyVisitedWebsite:
      return ash::OMNIBOX_RECENTLY_VISITED_WEBSITE;
    case CrosApiSearchResult::MetricsType::kHistoryTitle:
      return ash::OMNIBOX_RECENT_DOC_IN_DRIVE;
    case CrosApiSearchResult::MetricsType::kSearchWhatYouTyped:
      return ash::OMNIBOX_WEB_QUERY;
    case CrosApiSearchResult::MetricsType::kSearchHistory:
      return ash::OMNIBOX_SEARCH_HISTORY;
    case CrosApiSearchResult::MetricsType::kSearchSuggest:
      return ash::OMNIBOX_SEARCH_SUGGEST;
    case CrosApiSearchResult::MetricsType::kSearchSuggestPersonalized:
      return ash::OMNIBOX_SUGGEST_PERSONALIZED;
    case CrosApiSearchResult::MetricsType::kBookmark:
      return ash::OMNIBOX_BOOKMARK;
    // SEARCH_SUGGEST_ENTITY corresponds with rich entity results.
    case CrosApiSearchResult::MetricsType::kSearchSuggestEntity:
      return ash::OMNIBOX_SEARCH_SUGGEST_ENTITY;
    case CrosApiSearchResult::MetricsType::kNavSuggest:
      return ash::OMNIBOX_NAVSUGGEST;
    case CrosApiSearchResult::MetricsType::kCalculator:
      return ash::OMNIBOX_CALCULATOR;
    case CrosApiSearchResult::MetricsType::kUnset:
      return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }
}

void OmniboxResult::OnColorModeChanged(bool dark_mode_enabled) {
  if (uses_generic_icon_)
    SetGenericIcon();
}

void OmniboxResult::OnFaviconReceived(const gfx::ImageSkia& icon) {
  // By contract, this is never called with an empty |icon|.
  DCHECK(!icon.isNull());
  search_result_->favicon = icon;
  SetIcon(IconInfo(ui::ImageModel::FromImageSkia(icon), kFaviconDimension));
}

void OmniboxResult::UpdateIcon() {
  if (IsRichEntity()) {
    FetchRichEntityImage(*search_result_->image_url);
    return;
  }

  // Use a favicon if eligible. In the event that a favicon becomes available
  // asynchronously, it will be sent to us over Mojo and we will update our
  // icon.
  gfx::ImageSkia icon = search_result_->favicon;
  if (!icon.isNull()) {
    SetIcon(IconInfo(ui::ImageModel::FromImageSkia(icon), kFaviconDimension));
    return;
  }

  SetGenericIcon();
}

void OmniboxResult::SetGenericIcon() {
  uses_generic_icon_ = true;
  // If this is neither a rich entity nor eligible for a favicon, use either
  // the generic bookmark or another generic icon as appropriate.
  if (search_result_->omnibox_type ==
      CrosApiSearchResult::OmniboxType::kBookmark) {
    SetIcon(IconInfo(ui::ImageModel::FromVectorIcon(omnibox::kBookmarkIcon,
                                                    GetGenericIconColor(),
                                                    kSystemIconDimension),

                     kSystemIconDimension));
  } else {
    SetIcon(IconInfo(ui::ImageModel::FromVectorIcon(
                         TypeToVectorIcon(search_result_->omnibox_type),
                         GetGenericIconColor(), kSystemIconDimension),
                     kSystemIconDimension));
  }
}

void OmniboxResult::UpdateTitleAndDetails() {
  if (!IsUrlResultWithDescription()) {
    SetTitle(contents_);
    SetTitleTags(TagsForText(contents_, search_result_->contents_type));

    if (IsRichEntity()) {
      // Append the search engine to the description.
      const std::u16string description_with_search_context =
          l10n_util::GetStringFUTF16(
              IDS_APP_LIST_QUERY_SEARCH_DESCRIPTION, description_,
              GetDefaultSearchEngineName(
                  TemplateURLServiceFactory::GetForProfile(profile_)));
      SetDetails(description_with_search_context);
      SetDetailsTags(
          TagsForText(description_, search_result_->description_type));

      // Append the search engine to the accessible name.
      const std::u16string accessible_name =
          details().empty() ? title()
                            : base::StrCat({title(), u", ", details()});
      SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_QUERY_SEARCH_ACCESSIBILITY_NAME, accessible_name,
          GetDefaultSearchEngineName(
              TemplateURLServiceFactory::GetForProfile(profile_))));
    } else if (OptionalBoolIsTrue(search_result_->is_omnibox_search)) {
      // For non-rich-entity results, put the search engine into the details
      // field. Tags are not used since this does not change with the query.
      SetDetails(l10n_util::GetStringFUTF16(
          IDS_AUTOCOMPLETE_SEARCH_DESCRIPTION,
          GetDefaultSearchEngineName(
              TemplateURLServiceFactory::GetForProfile(profile_))));
    }
  } else {
    // For url result with non-empty description, swap title and details. Thus,
    // the url description is presented as title, and url itself is presented as
    // details.
    SetTitle(description_);
    SetTitleTags(TagsForText(description_, search_result_->description_type));

    SetDetails(contents_);
    SetDetailsTags(TagsForText(contents_, search_result_->contents_type));
  }
}

bool OmniboxResult::IsUrlResultWithDescription() const {
  return !(OptionalBoolIsTrue(search_result_->is_omnibox_search) ||
           description_.empty());
}

bool OmniboxResult::IsRichEntity() const {
  return search_result_->image_url.value_or(GURL()).is_valid();
}

void OmniboxResult::FetchRichEntityImage(const GURL& url) {
  if (!bitmap_fetcher_) {
    bitmap_fetcher_ =
        std::make_unique<BitmapFetcher>(url, this, kOmniboxTrafficAnnotation);
  }
  bitmap_fetcher_->Init(net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
  bitmap_fetcher_->Start(profile_->GetURLLoaderFactory().get());
}

void OmniboxResult::OnFetchComplete(const GURL& url, const SkBitmap* bitmap) {
  if (!bitmap)
    return;

  IconInfo icon_info(ui::ImageModel::FromImageSkia(
                         gfx::ImageSkia::CreateFrom1xBitmap(*bitmap)),
                     kImageIconDimension, IconShape::kRoundedRectangle);
  SetIcon(icon_info);
}

void OmniboxResult::InitializeButtonActions(
    const std::vector<ash::SearchResultActionType>& button_actions) {
  Actions actions;
  for (ash::SearchResultActionType button_action : button_actions) {
    std::u16string button_tooltip;
    switch (button_action) {
      case ash::SearchResultActionType::kRemove:
        button_tooltip = l10n_util::GetStringFUTF16(
            IDS_APP_LIST_REMOVE_SUGGESTION_ACCESSIBILITY_NAME, title());
        break;
    }
    Action search_action(button_action, button_tooltip);
    actions.emplace_back(search_action);
  }

  SetActions(actions);
}

}  // namespace app_list
