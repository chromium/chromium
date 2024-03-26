// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OPEN_TAB_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OPEN_TAB_RESULT_H_

#include <optional>
#include <string>

#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class AppListControllerDelegate;
class Profile;

namespace ash::string_matching {
class TokenizedString;
}

namespace app_list {

// Open tab search results. This is produced by the OmniboxProvider.
class OpenTabResult : public ChromeSearchResult,
                      public ash::ColorModeObserver,
                      public crosapi::mojom::SearchResultConsumer {
 public:
  OpenTabResult(Profile* profile,
                AppListControllerDelegate* list_controller,
                crosapi::mojom::SearchResultPtr search_result,
                const ash::string_matching::TokenizedString& query);
  ~OpenTabResult() override;

  OpenTabResult(const OpenTabResult&) = delete;
  OpenTabResult& operator=(const OpenTabResult&) = delete;

  // ChromeSearchResult:
  void Open(int event_flags) override;
  std::optional<GURL> url() const override;
  std::optional<std::string> DriveId() const override;

 private:
  // ash::ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

  void UpdateText();
  void UpdateIcon();
  // Creates a generic backup icon: used when rich icons are not available.
  void SetGenericIcon();

  // crosapi::mojom::SearchResultConsumer:
  void OnFaviconReceived(const gfx::ImageSkia& icon) override;

  // Handle used to receive a fetched favicon over mojo.
  const mojo::Receiver<crosapi::mojom::SearchResultConsumer> consumer_receiver_;

  const raw_ptr<Profile> profile_;
  const raw_ptr<AppListControllerDelegate> list_controller_;
  crosapi::mojom::SearchResultPtr search_result_;
  const std::optional<std::string> drive_id_;
  const std::u16string description_;
  // Whether this open tab result uses a generic backup icon.
  bool uses_generic_icon_ = false;

  base::WeakPtrFactory<OpenTabResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OPEN_TAB_RESULT_H_
