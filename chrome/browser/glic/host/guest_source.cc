// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_source.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/glic_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace glic {

std::string GetGuestAPISource() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string injected_client_js =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_GLIC_GLIC_API_IMPL_GLIC_API_INJECTED_CLIENT_ROLLUP_JS);

  auto config_dict =
      base::DictValue()
          .Set("loggingEnabled",
               command_line->HasSwitch(::switches::kGlicHostLogging))
          .Set("enableStructuredYieldMetadata",
               base::FeatureList::IsEnabled(
                   features::kGlicStructuredYieldMetadata));

  std::string config_json;
  base::JSONWriter::Write(config_dict, &config_json);

  return base::StrCat({"window.glicGuestLoadTimeData = ", config_json, ";\n",
                       injected_client_js});
}

}  // namespace glic
