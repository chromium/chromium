// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_answer_result.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/common/search_result_util.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/vector_icons.h"
#include "extensions/common/image_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using Tag = ash::SearchResultTag;
using TextItem = ash::SearchResultTextItem;
using TextType = ash::SearchResultTextItemType;
using CrosApiSearchResult = crosapi::mojom::SearchResult;

constexpr char kOmniboxAnswerSchema[] = "omnibox_answer://";

ChromeSearchResult::IconInfo CreateAnswerIconInfo(
    const gfx::VectorIcon& vector_icon) {
  const auto icon = gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      kAnswerCardIconDimension / 2, gfx::kGoogleBlue300,
      gfx::CreateVectorIcon(vector_icon, gfx::kGoogleGrey900));
  return ChromeSearchResult::IconInfo(ui::ImageModel::FromImageSkia(icon),
                                      kAnswerCardIconDimension);
}

// Convert from our Mojo answer type to the corresponding Omnibox icon.
const gfx::VectorIcon& AnswerTypeToVectorIcon(
    CrosApiSearchResult::AnswerType type) {
  switch (type) {
    case CrosApiSearchResult::AnswerType::kCurrency:
      return omnibox::kAnswerCurrencyIcon;
    case CrosApiSearchResult::AnswerType::kDictionary:
      return omnibox::kAnswerDictionaryIcon;
    case CrosApiSearchResult::AnswerType::kFinance:
      return omnibox::kAnswerFinanceIcon;
    case CrosApiSearchResult::AnswerType::kSunrise:
      return omnibox::kAnswerSunriseIcon;
    case CrosApiSearchResult::AnswerType::kTranslation:
      return omnibox::kAnswerTranslationIcon;
    case CrosApiSearchResult::AnswerType::kWhenIs:
      return omnibox::kAnswerWhenIsIcon;
    default:
      return omnibox::kAnswerDefaultIcon;
  }
}

// Tries to extract the temperature and temperature units from
// SuggestionAnswer::TextFields. For example, a text field containing "26°C" is
// converted into the pair ("26", "°C"), and a text field containing "-5°F" is
// converted into the pair ("-5", "°F").
std::optional<std::pair<std::u16string, std::u16string>> GetTemperature(
    const std::optional<std::u16string>& text) {
  if (!text.has_value() || text->empty())
    return std::nullopt;

  size_t digits_end = text->find_last_of(u"0123456789");
  if (digits_end == std::u16string::npos)
    return std::nullopt;

  size_t unit_start = digits_end + 1;
  return std::make_pair(text->substr(0, unit_start), text->substr(unit_start));
}

// Converts the given text into a TextItem and appends it to the supplied
// vector.
void AppendTextItem(const std::u16string& line,
                    const CrosApiSearchResult::TextType type,
                    std::vector<TextItem>& text_vector) {
  if (line.empty()) {
    return;
  }

  if (!text_vector.empty()) {
    text_vector.push_back(CreateStringTextItem(u" "));
  }

  TextItem text_item(TextType::kString);
  text_item.SetText(line);
  text_item.SetTextTags(TagsForText(line, type));
  text_vector.push_back(text_item);
}

std::u16string ComputeAccessibleName(
    const std::vector<std::vector<TextItem>>& text_vectors) {
  std::vector<std::u16string> text;
  for (const auto& text_vector : text_vectors) {
    if (!text_vector.empty()) {
      text.push_back(StringFromTextVector(text_vector));
    }
  }
  return base::JoinString(text, u", ");
}

}  // namespace

OmniboxAnswerResult::OmniboxAnswerResult(
    Profile* profile,
    AppListControllerDelegate* list_controller,
    crosapi::mojom::SearchResultPtr search_result,
    const std::u16string& query)
    : profile_(profile),
      list_controller_(list_controller),
      search_result_(std::move(search_result)),
      query_(query),
      contents_(search_result_->contents.value_or(u"")),
      additional_contents_(search_result_->additional_contents.value_or(u"")),
      description_(search_result_->description.value_or(u"")),
      additional_description_(
          search_result_->additional_description.value_or(u"")) {
  SetDisplayType(DisplayType::kAnswerCard);
  SetResultType(ResultType::kOmnibox);
  SetCategory(Category::kSearchAndAssistant);

  // mojo::StructPtr DCHECKs non-null on dereference.
  DCHECK(search_result_->stripped_destination_url.has_value());
  set_id(kOmniboxAnswerSchema +
         search_result_->stripped_destination_url->spec());

  SetMetricsType(IsCalculatorResult() ? ash::OMNIBOX_CALCULATOR
                                      : ash::OMNIBOX_ANSWER);

  // Derive relevance from omnibox relevance and normalize it to [0, 1].
  set_relevance(search_result_->relevance / kMaxOmniboxScore);

  UpdateIcon();
  UpdateTitleAndDetails();

  set_answer_type(search_result_->answer_type);

  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->AddObserver(this);
}

OmniboxAnswerResult::~OmniboxAnswerResult() {
  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->RemoveObserver(this);
}

void OmniboxAnswerResult::Open(int event_flags) {
  DCHECK(search_result_->destination_url.has_value());
  list_controller_->OpenURL(
      profile_, *search_result_->destination_url,
      PageTransitionToUiPageTransition(search_result_->page_transition),
      ui::DispositionFromEventFlags(event_flags));
}

void OmniboxAnswerResult::OnColorModeChanged(bool dark_mode_enabled) {
  if (!IsWeatherResult())
    UpdateIcon();
}

void OmniboxAnswerResult::UpdateIcon() {
  if (IsCalculatorResult()) {
    SetIcon(CreateAnswerIconInfo(omnibox::kCalculatorIcon));
  } else if (IsWeatherResult() &&
             search_result_->image_url.value_or(GURL()).is_valid()) {
    // Weather icons are downloaded. Check this first so that the local
    // default answer icon can be used as a fallback if the URL is missing.
    FetchImage(*search_result_->image_url);
  } else {
    SetIcon(CreateAnswerIconInfo(
        AnswerTypeToVectorIcon(search_result_->answer_type)));
  }
}

void OmniboxAnswerResult::UpdateTitleAndDetails() {
  if (IsWeatherResult()) {
    auto temperature = GetTemperature(search_result_->description);
    if (temperature) {
      SetBigTitleTextVector({CreateStringTextItem(temperature->first)});
      SetBigTitleSuperscriptTextVector(
          {CreateStringTextItem(temperature->second)});
    } else {
      // If the temperature can't be parsed, don't display this result.
      scoring().set_filtered(true);
    }

    if (search_result_->description_a11y_label.has_value()) {
      SetTitleTextVector({CreateStringTextItem(
          search_result_->description_a11y_label.value())});
    } else {
      std::vector<TextItem> title_vector;
      AppendTextItem(contents_, search_result_->contents_type, title_vector);
      SetTitleTextVector(title_vector);
    }

    std::vector<TextItem> details_vector;
    AppendTextItem(additional_description_,
                   search_result_->additional_description_type, details_vector);
    SetDetailsTextVector(details_vector);
  } else {
    // Not a weather result.

    std::vector<TextItem> title_vector;
    AppendTextItem(contents_, search_result_->contents_type, title_vector);
    AppendTextItem(additional_contents_,
                   search_result_->additional_contents_type, title_vector);
    SetTitleTextVector(title_vector);

    std::vector<TextItem> details_vector;
    AppendTextItem(description_, search_result_->description_type,
                   details_vector);
    AppendTextItem(additional_description_,
                   search_result_->additional_description_type, details_vector);
    SetDetailsTextVector(details_vector);

    // Dictionary answer details can be split over multiple lines.
    if (IsDictionaryResult())
      SetMultilineDetails(true);
  }

  const std::u16string accessible_name = ComputeAccessibleName(
      {big_title_text_vector(), title_text_vector(), details_text_vector()});
  SetAccessibleName(accessible_name);
}

void OmniboxAnswerResult::FetchImage(const GURL& url) {
  if (!bitmap_fetcher_) {
    bitmap_fetcher_ =
        std::make_unique<BitmapFetcher>(url, this, kOmniboxTrafficAnnotation);
  }
  bitmap_fetcher_->Init(net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
  bitmap_fetcher_->Start(profile_->GetURLLoaderFactory().get());
}

void OmniboxAnswerResult::OnFetchComplete(const GURL& url,
                                          const SkBitmap* bitmap) {
  if (!bitmap)
    return;

  DCHECK(IsWeatherResult());
  IconInfo icon_info(ui::ImageModel::FromImageSkia(
                         gfx::ImageSkia::CreateFrom1xBitmap(*bitmap)),
                     kAnswerCardIconDimension);
  SetIcon(icon_info);
}

bool OmniboxAnswerResult::IsCalculatorResult() const {
  return search_result_->answer_type ==
         CrosApiSearchResult::AnswerType::kCalculator;
}

bool OmniboxAnswerResult::IsDictionaryResult() const {
  return search_result_->answer_type ==
         CrosApiSearchResult::AnswerType::kDictionary;
}

bool OmniboxAnswerResult::IsWeatherResult() const {
  return search_result_->answer_type ==
         CrosApiSearchResult::AnswerType::kWeather;
}

}  // namespace app_list
