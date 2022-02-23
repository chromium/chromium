// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/help_content_provider.h"

#include <memory>

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

using os_feedback_ui::mojom::HelpContent;
using os_feedback_ui::mojom::HelpContentType;
using os_feedback_ui::mojom::SearchResponse;
using os_feedback_ui::mojom::SearchResponsePtr;

HelpContentSearchService::HelpContentSearchService() = default;
HelpContentSearchService::~HelpContentSearchService() = default;

void HelpContentSearchService::Search(
    const os_feedback_ui::mojom::SearchRequestPtr& request,
    os_feedback_ui::mojom::SearchResponsePtr& response) {
  // TODO(xiagndongkong): implement the search and populate response.
  response->total_results = 0;
  // TODO(xiangdongkong): Remove the following dummy item.
  response->results.emplace_back(HelpContent::New(u"how to fix wifi issue",
                                                  GURL("https://fakehelp.com"),
                                                  HelpContentType::kArticle));
}

HelpContentProvider::HelpContentProvider()
    : HelpContentProvider(std::make_unique<HelpContentSearchService>()) {}

HelpContentProvider::HelpContentProvider(
    std::unique_ptr<HelpContentSearchService> service)
    : help_content_service_(std::move(service)) {}

HelpContentProvider::~HelpContentProvider() = default;

void HelpContentProvider::GetHelpContents(
    os_feedback_ui::mojom::SearchRequestPtr request,
    GetHelpContentsCallback callback) {
  SearchResponsePtr response = SearchResponse::New();
  help_content_service_->Search(request, response);

  std::move(callback).Run(std::move(response));
}

void HelpContentProvider::BindInterface(
    mojo::PendingReceiver<os_feedback_ui::mojom::HelpContentProvider>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

}  // namespace feedback
}  // namespace ash
