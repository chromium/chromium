// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_PROVIDER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_PROVIDER_H_

#include <optional>

#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "components/manta/manta_service_callbacks.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace ash::input_method {

class EditorTextQueryProvider : public orca::mojom::TextQueryProvider {
 public:
  class MantaProvider {
   public:
    virtual ~MantaProvider() = default;

    virtual void Call(const std::map<std::string, std::string> params,
                      manta::MantaGenericCallback callback) = 0;
  };

  EditorTextQueryProvider(
      mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider> receiver,
      EditorMetricsRecorder* metrics_recorder,
      std::unique_ptr<MantaProvider> manta_provider);

  ~EditorTextQueryProvider() override;

  // orca::mojom::TextQueryProvider overrides
  void Process(orca::mojom::TextQueryRequestPtr request,
               ProcessCallback callback) override;

  // Override the provider used to generate responses.
  void SetProvider(std::unique_ptr<MantaProvider> provider);

 private:
  mojo::AssociatedReceiver<orca::mojom::TextQueryProvider>
      text_query_provider_receiver_;

  // not owned by this class
  raw_ptr<EditorMetricsRecorder> metrics_recorder_;

  std::unique_ptr<MantaProvider> manta_provider_;

  // Unsigned to allow safe overflows.
  unsigned int request_id_ = 0;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_PROVIDER_H_
