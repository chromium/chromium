// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_PROVIDER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_PROVIDER_H_

#include <optional>

#include "chrome/browser/ash/input_method/editor_switch.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "components/manta/orca_provider.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace ash::input_method {

class EditorTextQueryProvider : public orca::mojom::TextQueryProvider {
 public:
  // orca::mojom::TextQueryProvider overrides
  void Process(orca::mojom::TextQueryRequestPtr request,
               ProcessCallback callback) override = 0;

  virtual std::optional<
      mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider>>
  Unbind() = 0;
};

class TextQueryProviderForOrca : public EditorTextQueryProvider {
 public:
  TextQueryProviderForOrca(
      mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider> receiver,
      Profile* profile,
      EditorSwitch* editor_switch);

  ~TextQueryProviderForOrca() override;

  // EditorTextQueryProvider overrides
  void Process(orca::mojom::TextQueryRequestPtr request,
               ProcessCallback callback) override;

  std::optional<mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider>>
  Unbind() override;

 private:
  mojo::AssociatedReceiver<orca::mojom::TextQueryProvider>
      text_query_provider_receiver_;
  std::unique_ptr<manta::OrcaProvider> orca_provider_;

  // not owned by this class
  raw_ptr<EditorSwitch> editor_switch_;

  // Unsigned to allow safe overflows.
  unsigned int request_id_ = 0;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_PROVIDER_H_
