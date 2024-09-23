// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_geolocation_provider_from_finch.h"

#include "chrome/browser/browser_process.h"
#include "components/variations/service/variations_service.h"

namespace ash::input_method {

std::string EditorGeolocationProviderFromFinch::GetCountryCode() {
  return (g_browser_process != nullptr &&
          g_browser_process->variations_service() != nullptr)
             ? g_browser_process->variations_service()->GetLatestCountry()
             : "";
}

}  // namespace ash::input_method
