// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SUGGESTION_SERVICE_LACROS_H_
#define CHROME_BROWSER_LACROS_SUGGESTION_SERVICE_LACROS_H_

#include "chromeos/crosapi/mojom/suggestion_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class SuggestionServiceLacros
    : public crosapi::mojom::SuggestionServiceProvider {
 public:
  SuggestionServiceLacros();
  SuggestionServiceLacros(const SuggestionServiceLacros&) = delete;
  SuggestionServiceLacros& operator=(const SuggestionServiceLacros&) = delete;
  ~SuggestionServiceLacros() override;

  // SuggestionServiceProvider:
  void GetTabSuggestionItems(GetTabSuggestionItemsCallback callback) override;

 private:
  mojo::Receiver<crosapi::mojom::SuggestionServiceProvider> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_SUGGESTION_SERVICE_LACROS_H_
