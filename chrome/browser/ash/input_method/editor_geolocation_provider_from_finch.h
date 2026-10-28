// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_PROVIDER_FROM_FINCH_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_PROVIDER_FROM_FINCH_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"
#include "components/variations/service/variations_service.h"

namespace ash::input_method {

class EditorGeolocationProviderFromFinch
    : public EditorGeolocationProvider,
      public variations::VariationsService::Observer {
 public:
  // `variations_service` must not be null.
  explicit EditorGeolocationProviderFromFinch(
      variations::VariationsService* variations_service);
  ~EditorGeolocationProviderFromFinch() override;

  std::string GetCountryCode() override;

  // variations::VariationsService::Observer:
  void OnVariationsServiceDestroyed() override;

 private:
  raw_ptr<variations::VariationsService> variations_service_ = nullptr;
  base::ScopedObservation<variations::VariationsService,
                          variations::VariationsService::Observer>
      observation_{this};
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_GEOLOCATION_PROVIDER_FROM_FINCH_H_
