// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_SERVICE_CLIENT_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/input_method/suggestions_source.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"

namespace chromeos {

class SuggestionsServiceClient : public AsyncSuggestionsSource {
 public:
  SuggestionsServiceClient();

  // AsyncSuggestionsSource overrides
  void GetSuggestions(GetSuggestionsCallback callback) override;
  bool IsAvailable() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_SERVICE_CLIENT_H_
