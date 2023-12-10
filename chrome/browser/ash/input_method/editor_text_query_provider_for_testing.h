// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_PROVIDER_FOR_TESTING_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_PROVIDER_FOR_TESTING_H_

#include <optional>
#include <vector>

#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace ash::input_method {

class TextQueryProviderForTesting : public EditorTextQueryProvider {
 public:
  TextQueryProviderForTesting(
      mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider> receiver,
      const std::vector<std::string>& mock_responses);

  ~TextQueryProviderForTesting() override;

  // EditorTextQueryProvider overrides
  void Process(orca::mojom::TextQueryRequestPtr request,
               ProcessCallback callback) override;

  std::optional<mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider>>
  Unbind() override;

 private:
  mojo::AssociatedReceiver<orca::mojom::TextQueryProvider>
      text_query_provider_receiver_;
  std::vector<std::string> mock_responses_;
};

}  // namespace ash::input_method
#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_PROVIDER_FOR_TESTING_H_
