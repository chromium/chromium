// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_FROM_MEMORY_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_FROM_MEMORY_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "components/manta/manta_service_callbacks.h"

namespace ash::input_method {

class EditorTextQueryFromMemory
    : public EditorTextQueryProvider::MantaProvider {
 public:
  explicit EditorTextQueryFromMemory(base::span<const std::string> responses);
  ~EditorTextQueryFromMemory() override;

  // EditorTextQueryProvider::MantaProvider overrides
  void Call(const std::map<std::string, std::string> params,
            manta::MantaGenericCallback callback) override;

 private:
  std::vector<std::string> responses_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_TEXT_QUERY_FROM_MEMORY_H_
