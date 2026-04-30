// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_extension_utils.h"

#include <iterator>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/i18n/rtl.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/indigo/resources/grit/indigo_browser_resources.h"
#include "chrome/browser/indigo/resources/grit/indigo_browser_resources_map.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/indigo_resources.h"
#include "chrome/grit/indigo_resources_map.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace indigo_extension_utils {

std::string GetManifest() {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_INDIGO_MANIFEST_JSON);
}

base::DictValue GetStrings() {
  std::string application_locale =
      g_browser_process
          ? g_browser_process->GetFeatures()->application_locale_storage()->Get(
                ApplicationLocaleStorage::LocaleFormat::kBCP47)
          : "en-US";

  base::DictValue dict;
  dict.Set("textdirection", base::i18n::IsRTL() ? "rtl" : "ltr");
  dict.Set("language", l10n_util::GetLanguage(application_locale));
  dict.Set("indigoTitle", l10n_util::GetStringUTF16(IDS_INDIGO_TITLE));
  dict.Set("textLayerStep1",
           l10n_util::GetStringUTF16(IDS_INDIGO_TEXT_LAYER_STEP_1));
  dict.Set("textLayerStep2",
           l10n_util::GetStringUTF16(IDS_INDIGO_TEXT_LAYER_STEP_2));
  dict.Set("textLayerStep3",
           l10n_util::GetStringUTF16(IDS_INDIGO_TEXT_LAYER_STEP_3));
  return dict;
}

base::span<const webui::ResourcePath> GetResources() {
  static const base::NoDestructor<std::vector<webui::ResourcePath>>
      kAllResources([] {
        std::vector<webui::ResourcePath> resources;
        resources.reserve(std::size(kIndigoResources) +
                          std::size(kIndigoBrowserResources));
        for (const auto& resource : kIndigoResources) {
          resources.push_back(resource);
        }
        for (const auto& resource : kIndigoBrowserResources) {
          resources.push_back(resource);
        }
        return resources;
      }());
  return *kAllResources;
}

}  // namespace indigo_extension_utils
