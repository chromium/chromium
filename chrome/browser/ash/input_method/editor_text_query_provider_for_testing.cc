// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_query_provider_for_testing.h"

#include <optional>

#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace ash::input_method {

TextQueryProviderForTesting::TextQueryProviderForTesting(
    mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider> receiver,
    const std::vector<std::string>& mock_responses)
    : text_query_provider_receiver_(this, std::move(receiver)),
      mock_responses_(mock_responses) {}

TextQueryProviderForTesting::~TextQueryProviderForTesting() =
    default;  // IN-TEST

void TextQueryProviderForTesting::Process(
    orca::mojom::TextQueryRequestPtr request,
    ProcessCallback callback) {
  std::vector<ash::orca::mojom::TextQueryResultPtr> responses;
  for (size_t i = 0; i < mock_responses_.size(); ++i) {
    // fetch the responses with mock request_id and result_id
    responses.push_back(ash::orca::mojom::TextQueryResult::New(
        base::StrCat({base::NumberToString(i), ":", base::NumberToString(i)}),
        mock_responses_[i]));
  }

  auto results =
      orca::mojom::TextQueryResponse::NewResults(std::move(responses));
  std::move(callback).Run(std::move(results));
}

std::optional<mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider>>
TextQueryProviderForTesting::Unbind() {
  if (text_query_provider_receiver_.is_bound()) {
    return text_query_provider_receiver_.Unbind();
  }
  return std::nullopt;
}
}  // namespace ash::input_method
