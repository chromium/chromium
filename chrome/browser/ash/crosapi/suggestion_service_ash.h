// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SUGGESTION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SUGGESTION_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/suggestion_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

using TabSuggestionItemsCallback =
    base::OnceCallback<void(std::vector<mojom::TabSuggestionItemPtr>)>;

class SuggestionServiceAsh : public mojom::SuggestionService {
 public:
  SuggestionServiceAsh();
  SuggestionServiceAsh(const SuggestionServiceAsh&) = delete;
  SuggestionServiceAsh& operator=(const SuggestionServiceAsh&) = delete;
  ~SuggestionServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::SuggestionService> receiver);

  // mojom::SuggestionService:
  void AddSuggestionServiceProvider(
      mojo::PendingRemote<mojom::SuggestionServiceProvider> provider) override;

  void GetTabSuggestionItems(TabSuggestionItemsCallback callback);

 private:
  mojo::ReceiverSet<mojom::SuggestionService> receivers_;

  // Each separate Lacros process owns its own remote.
  mojo::RemoteSet<mojom::SuggestionServiceProvider> remotes_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SUGGESTION_SERVICE_ASH_H_
