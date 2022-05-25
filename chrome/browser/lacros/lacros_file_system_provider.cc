// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_file_system_provider.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace {

// Returns the single main profile, or nullptr if none is found.
Profile* GetMainProfile() {
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  const auto main_it = base::ranges::find_if(
      profiles, [](Profile* profile) { return profile->IsMainProfile(); });
  if (main_it == profiles.end())
    return nullptr;
  return *main_it;
}
}  // namespace

LacrosFileSystemProvider::LacrosFileSystemProvider() : receiver_{this} {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::FileSystemProviderService>())
    return;
  service->GetRemote<crosapi::mojom::FileSystemProviderService>()
      ->RegisterFileSystemProvider(receiver_.BindNewPipeAndPassRemote());
}
LacrosFileSystemProvider::~LacrosFileSystemProvider() = default;

void LacrosFileSystemProvider::ForwardOperation(const std::string& provider,
                                                int32_t histogram_value,
                                                const std::string& event_name,
                                                std::vector<base::Value> args) {
  Profile* main_profile = GetMainProfile();
  if (!main_profile)
    return;

  extensions::EventRouter* router = extensions::EventRouter::Get(main_profile);
  if (!router)
    return;

  if (!router->ExtensionHasEventListener(provider, event_name))
    return;

  // Conversions are safe since the enum is stable. See documentation.
  int32_t lowest_valid_enum =
      static_cast<int32_t>(extensions::events::HistogramValue::UNKNOWN);
  int32_t highest_valid_enum =
      static_cast<int32_t>(extensions::events::HistogramValue::ENUM_BOUNDARY) -
      1;
  if (histogram_value < lowest_valid_enum ||
      histogram_value > highest_valid_enum) {
    return;
  }
  extensions::events::HistogramValue histogram =
      static_cast<extensions::events::HistogramValue>(histogram_value);

  auto event = std::make_unique<extensions::Event>(histogram, event_name,
                                                   std::move(args));
  router->DispatchEventToExtension(provider, std::move(event));
}
