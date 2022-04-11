// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/search/search_handler.h"

#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace personalization_app {

SearchHandler::SearchHandler() = default;

SearchHandler::~SearchHandler() = default;

void SearchHandler::BindInterface(
    mojo::PendingReceiver<mojom::SearchHandler> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SearchHandler::Search(const std::u16string& query,
                           SearchCallback callback) {
  std::vector<::ash::personalization_app::mojom::SearchResultPtr> results;

  // TODO(b/225950660) add real search results.
  std::u16string title = l10n_util::GetStringUTF16(
      IDS_PERSONALIZATION_APP_PERSONALIZATION_HUB_TITLE);
  if (query == title) {
    results.push_back(::ash::personalization_app::mojom::SearchResult::New(
        title, /*relative_url=*/std::string(), /*relevance_score=*/1.0));
  }

  std::move(callback).Run(std::move(results));
}

}  // namespace personalization_app
}  // namespace ash
