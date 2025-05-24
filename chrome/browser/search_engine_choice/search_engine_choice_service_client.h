// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_CLIENT_H_
#define CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_CLIENT_H_

#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"

class Profile;

namespace search_engines {

// Provides access to //chrome layer-specific state and utilities to
// `SearchEngineChoiceService`.
class SearchEngineChoiceServiceClient
    : public SearchEngineChoiceService::Client {
 public:
  explicit SearchEngineChoiceServiceClient(const Profile& profile);

  // `SearchEngineChoiceService::Client` implementation.
  country_codes::CountryId GetVariationsCountry() override;
  bool IsProfileEligibleForDseGuestPropagation() override;
  bool IsDeviceRestoreDetectedInCurrentSession() override;
  bool DoesChoicePredateDeviceRestore(
      const ChoiceCompletionMetadata& choice_metadata) override;

 private:
  bool is_profile_eligible_for_dse_guest_propagation_;
};

}  // namespace search_engines

#endif  // CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_CLIENT_H_
