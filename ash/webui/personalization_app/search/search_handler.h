// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_HANDLER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_HANDLER_H_

#include <string>

#include "ash/webui/personalization_app/search/search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {
namespace personalization_app {

class SearchHandler : public mojom::SearchHandler {
 public:
  SearchHandler();

  SearchHandler(const SearchHandler& other) = delete;
  SearchHandler& operator=(const SearchHandler& other) = delete;

  ~SearchHandler() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SearchHandler> pending_receiver);

  // mojom::SearchHandler:
  void Search(const std::u16string& query, SearchCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::SearchHandler> receivers_;
};

}  // namespace personalization_app
}  // namespace ash

#endif  // ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_HANDLER_H_
