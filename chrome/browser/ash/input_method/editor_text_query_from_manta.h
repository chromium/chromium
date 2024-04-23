// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_FROM_MANTA_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_FROM_MANTA_H_

#include <map>
#include <string>

#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/orca_provider.h"

namespace ash::input_method {

class EditorTextQueryFromManta : public EditorTextQueryProvider::MantaProvider {
 public:
  explicit EditorTextQueryFromManta(Profile* profile);
  ~EditorTextQueryFromManta() override;

  // EditorTextQueryProvider::MantaProvider overrides
  void Call(const std::map<std::string, std::string> params,
            manta::MantaGenericCallback callback) override;

 private:
  std::unique_ptr<manta::OrcaProvider> provider_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_FROM_MANTA_H_
