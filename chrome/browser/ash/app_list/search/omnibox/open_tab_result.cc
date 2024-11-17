// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/open_tab_result.h"

#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/common/search_result_util.h"
#include "chrome/browser/ash/app_list/search/common/string_util.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/string_matching/tokenized_string_match.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace app_list {

namespace {

using ::ash::string_matching::TokenizedString;
using ::ash::string_matching::TokenizedStringMatch;
using CrosApiSearchResult = ::crosapi::mojom::SearchResult;

constexpr char kOpenTabScheme[] = "opentab://";

constexpr char16_t kUrlDelimiter[] = u" - ";

constexpr char16_t kA11yDelimiter[] = u", ";

}  // namespace

OpenTabResult::OpenTabResult(Profile* profile,
                             AppListControllerDelegate* list_controller,
                             crosapi::mojom::SearchResultPtr search_result,
                             const TokenizedString& query)
    : consumer_receiver_(this, std::move(search_result->receiver)),
      profile_(profile),
      list_controller_(list_controller),
      search_result_(std::move(search_result)),
      drive_id_(GetDriveId(*search_result_->destination_url)),
      description_(search_result_->description.value_or(u"")) {
  DCHECK(search_result_->destination_url->is_valid());

  // TODO(crbug.com/1293702): This may not be unique. Once we have a mechanism
  // for opening a specific tab, add that info too to ensure uniqueness.
  set_id(kOpenTabScheme + search_result_->destination_url->spec());

  SetDisplayType(DisplayType::kList);
  SetResultType(ResultType::kOpenTab);
  SetMetricsType(ash::OPEN_TAB);
  SetCategory(Category::kWeb);

  // Ignore `search_result_->relevance` and manually calculate a relevance
  // score for this result.
  TokenizedStringMatch string_match;
  TokenizedString title(description_);
  set_relevance(string_match.Calculate(query, title));

  UpdateText();
  UpdateIcon();
  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->AddObserver(this);
}

OpenTabResult::~OpenTabResult() {
  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->RemoveObserver(this);
}

void OpenTabResult::Open(int event_flags) {
  list_controller_->OpenURL(
      profile_, *search_result_->destination_url,
      PageTransitionToUiPageTransition(search_result_->page_transition),
      ui::DispositionFromEventFlags(event_flags,
                                    WindowOpenDisposition::SWITCH_TO_TAB));
}

std::optional<GURL> OpenTabResult::url() const {
  return *search_result_->destination_url;
}

std::optional<std::string> OpenTabResult::DriveId() const {
  return drive_id_;
}

void OpenTabResult::OnColorModeChanged(bool dark_mode_enabled) {
  if (uses_generic_icon_)
    SetGenericIcon();
}

void OpenTabResult::UpdateText() {
  // URL results from the Omnibox have the page title stored in the description.
  SetTitle(description_);

  const std::u16string url =
      base::UTF8ToUTF16(search_result_->destination_url->spec());
  SetDetailsTextVector(
      {CreateStringTextItem(url).SetTextTags({Tag(Tag::URL, 0, url.length())}),
       CreateStringTextItem(l10n_util::GetStringFUTF16(
                                IDS_APP_LIST_OPEN_TAB_HINT, kUrlDelimiter))
           .SetOverflowBehavior(
               ash::SearchResultTextItem::OverflowBehavior::kNoElide)});

  SetAccessibleName(base::JoinString(
      {description_, url,
       l10n_util::GetStringFUTF16(IDS_APP_LIST_OPEN_TAB_HINT, u"")},
      kA11yDelimiter));
}

void OpenTabResult::UpdateIcon() {
  // Use a favicon if one is available.
  if (!search_result_->favicon.isNull()) {
    SetIcon(IconInfo(ui::ImageModel::FromImageSkia(search_result_->favicon),
                     kFaviconDimension));
    return;
  }

  // Otherwise, fall back to using a generic icon.
  // TODO(crbug.com/1293702): WIP. Decide on the right generic icon here.
  SetGenericIcon();
}

void OpenTabResult::SetGenericIcon() {
  uses_generic_icon_ = true;
  SetIcon(IconInfo(
      ui::ImageModel::FromVectorIcon(
          omnibox::kSwitchIcon, GetGenericIconColor(), kSystemIconDimension),
      kSystemIconDimension));
}

void OpenTabResult::OnFaviconReceived(const gfx::ImageSkia& icon) {
  // By contract, this is never called with an empty `icon`.
  DCHECK(!icon.isNull());
  search_result_->favicon = icon;
  SetIcon(IconInfo(ui::ImageModel::FromImageSkia(icon), kFaviconDimension));
}

}  // namespace app_list
