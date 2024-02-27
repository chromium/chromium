// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PICKER_PICKER_CLIENT_H_
#define ASH_PUBLIC_CPP_PICKER_PICKER_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// Lets PickerController in Ash to communicate with the browser.
class ASH_PUBLIC_EXPORT PickerClient {
 public:
  using FetchGifsCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult> results)>;
  using CrosSearchResultsCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType result_type,
                                   std::vector<PickerSearchResult> results)>;

  virtual std::unique_ptr<ash::AshWebView> CreateWebView(
      const ash::AshWebView::InitParams& params) = 0;

  // Gets the SharedURLLoaderFactory to use for Picker network requests, e.g. to
  // fetch assets.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSharedURLLoaderFactory() = 0;

  // Fetches a list of gifs from the Tenor API.
  virtual void FetchGifSearch(const std::string& query,
                              FetchGifsCallback callback) = 0;

  // Stops the current `FetchGifSearch` network request. Any callbacks will not
  // be called.
  virtual void StopGifSearch() = 0;

  // Starts a search using the CrOS Search API
  // (`app_list::SearchEngine::StartSearch`).
  virtual void StartCrosSearch(const std::u16string& query,
                               std::optional<PickerCategory> category,
                               CrosSearchResultsCallback callback) = 0;
  // Stops a search using the CrOS Search API
  // (`app_list::SearchEngine::StopQuery`).
  virtual void StopCrosQuery() = 0;

 protected:
  PickerClient();
  virtual ~PickerClient();
};

}  // namespace ash

#endif
