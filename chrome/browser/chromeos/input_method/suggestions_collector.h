// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/input_method/suggestions_source.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"

namespace chromeos {

// Used to collect any text suggestions from the system
class SuggestionsCollector {
 public:
  // `assistive_suggester` must exist for the lifetime of this instance.
  explicit SuggestionsCollector(SuggestionsSource* assistive_suggester);

  using GatherSuggestionsCallback =
      base::OnceCallback<void(ime::mojom::SuggestionsResponsePtr)>;

  // Collects all suggestions from the system.
  void GatherSuggestions(ime::mojom::SuggestionsRequestPtr request,
                         GatherSuggestionsCallback callback);

 private:
  // Not owned by this class
  SuggestionsSource* assistive_suggester_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_
