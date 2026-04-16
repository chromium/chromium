// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_extension_utils.h"

#include <iterator>
#include <string>

#include "base/i18n/rtl.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
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
  return dict;
}

base::span<const webui::ResourcePath> GetResources() {
  return base::span<const webui::ResourcePath>(kIndigoResources);
}

}  // namespace indigo_extension_utils
