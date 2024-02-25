// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_ANSWER_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_ANSWER_RESULT_H_

#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"

class AppListControllerDelegate;
class BitmapFetcher;
class Profile;

namespace app_list {

// An answer result from the Omnibox provider. Answer results are displayed as
// cards at the top of the categorical search launcher, so are separated in the
// UI from other Omnibox results.
class OmniboxAnswerResult : public ChromeSearchResult,
                            public BitmapFetcherDelegate,
                            public ash::ColorModeObserver {
 public:
  OmniboxAnswerResult(Profile* profile,
                      AppListControllerDelegate* list_controller,
                      crosapi::mojom::SearchResultPtr search_result,
                      const std::u16string& query);
  ~OmniboxAnswerResult() override;

  OmniboxAnswerResult(const OmniboxAnswerResult&) = delete;
  OmniboxAnswerResult& operator=(const OmniboxAnswerResult&) = delete;

  // ChromeSearchResult:
  void Open(int event_flags) override;

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

 private:
  // ash::ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

  void UpdateIcon();
  void UpdateTitleAndDetails();

  void FetchImage(const GURL& url);

  bool IsCalculatorResult() const;
  bool IsDictionaryResult() const;
  bool IsWeatherResult() const;

  raw_ptr<Profile> profile_;
  raw_ptr<AppListControllerDelegate> list_controller_;
  const crosapi::mojom::SearchResultPtr search_result_;
  const std::u16string query_;
  std::unique_ptr<BitmapFetcher> bitmap_fetcher_;

  // Cached unwrapped search result fields.
  const std::u16string contents_;
  const std::u16string additional_contents_;
  const std::u16string description_;
  const std::u16string additional_description_;

  base::WeakPtrFactory<OmniboxAnswerResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_ANSWER_RESULT_H_
