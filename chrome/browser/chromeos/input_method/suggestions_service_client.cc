// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_service_client.h"

namespace chromeos {

SuggestionsServiceClient::SuggestionsServiceClient() {
  // TODO(crbug/1146266): Complete setup of text suggestion service client.
}

void SuggestionsServiceClient::GetSuggestions(GetSuggestionsCallback callback) {
  std::move(callback).Run(std::vector<ime::TextSuggestion>{});
}

bool SuggestionsServiceClient::IsAvailable() {
  return false;
}

}  // namespace chromeos
