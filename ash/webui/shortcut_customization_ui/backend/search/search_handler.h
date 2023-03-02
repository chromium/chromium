// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_HANDLER_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_HANDLER_H_

#include <vector>

#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::shortcut_ui {

// Handles search queries for the ChromeOS Shortcuts app.
//
// Search() is expected to be invoked by the Shortcuts UI as well as the
// Launcher search UI.
//
// Search results are obtained by matching the provided query against search
// tags indexed in the LocalSearchService and cross-referencing results with
// SearchTagRegistry.
//
// Searches which do not provide any matches result in an empty results array.
class SearchHandler : public shortcut_customization::mojom::SearchHandler {
 public:
  SearchHandler();
  ~SearchHandler() override;

  SearchHandler(const SearchHandler& other) = delete;
  SearchHandler& operator=(const SearchHandler& other) = delete;

  void BindInterface(
      mojo::PendingReceiver<shortcut_customization::mojom::SearchHandler>
          pending_receiver);

  // shortcut_customization::mojom::SearchHandler:
  void Search(const std::u16string& query,
              uint32_t max_num_results,
              SearchCallback callback) override;

 private:
  // Note: Expected to have multiple clients, so ReceiverSet/RemoteSet are used.
  mojo::ReceiverSet<shortcut_customization::mojom::SearchHandler> receivers_;
};

}  // namespace ash::shortcut_ui

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SEARCH_SEARCH_HANDLER_H_