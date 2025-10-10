// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/shared/webui_shared.h"

#include "base/command_line.h"
#include "base/version_info/version_info.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/glic_shared_resources.h"
#include "chrome/grit/glic_shared_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

namespace glic {

void ConfigureSharedWebUISource(content::WebUIDataSource& source) {
  source.AddString("chromeVersion", version_info::GetVersionNumber());
  source.AddString("chromeChannel",
                   version_info::GetChannelString(chrome::GetChannel()));

  auto* command_line = base::CommandLine::ForCurrentProcess();
  const bool is_glic_dev = command_line->HasSwitch(::switches::kGlicDev);

  source.AddBoolean("devMode", is_glic_dev);

  // Set up loading notice timeout values.
  source.AddInteger("preLoadingTimeMs", features::kGlicPreLoadingTimeMs.Get());
  source.AddInteger("minLoadingTimeMs", features::kGlicMinLoadingTimeMs.Get());
  int max_loading_time_ms = features::kGlicMaxLoadingTimeMs.Get();
  if (is_glic_dev) {
    // Bump up timeout value, as dev server may be slow.
    max_loading_time_ms *= 100;
  }
  source.AddInteger("maxLoadingTimeMs", max_loading_time_ms);

  source.AddString("glicHeaderRequestTypes",
                   base::FeatureList::IsEnabled(features::kGlicHeader)
                       ? features::kGlicHeaderRequestTypes.Get()
                       : "");

  source.AddResourcePaths(kGlicSharedResources);
}

}  // namespace glic
