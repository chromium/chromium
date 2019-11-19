// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/heavy_ad_intervention/heavy_ad_helper.h"

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace heavy_ads {

std::string PrepareHeavyAdPage() {
  int resource_id = IDR_SECURITY_INTERSTITIAL_QUIET_HTML;
  std::string uncompressed;
  base::StringPiece template_html(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  if (ui::ResourceBundle::GetSharedInstance().IsGzipped(resource_id)) {
    bool success = compression::GzipUncompress(template_html, &uncompressed);
    DCHECK(success);
    template_html = base::StringPiece(uncompressed);
  }
  DCHECK(!template_html.empty()) << "unable to load template.";

  // Populate load time data.
  base::DictionaryValue load_time_data;
  load_time_data.SetString("type", "HEAVYAD");
  load_time_data.SetString(
      "heading", l10n_util::GetStringUTF16(IDS_HEAVY_AD_INTERVENTION_HEADING));
  load_time_data.SetString(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_HEAVY_AD_INTERVENTION_BUTTON_DETAILS));
  load_time_data.SetString(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_HEAVY_AD_INTERVENTION_SUMMARY));

  // Ad frames are never the main frame, so we do not need a tab title.
  load_time_data.SetString("tabTitle", "");
  load_time_data.SetBoolean("overridable", false);
  load_time_data.SetBoolean("is_giant", false);

  security_interstitials::common_string_util::PopulateDarkModeDisplaySetting(
      &load_time_data);

  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 &load_time_data);

  // "body" is the id of the template's root node.
  std::string heavy_ad_html =
      webui::GetTemplatesHtml(template_html, &load_time_data, "body");
  webui::AppendWebUiCssTextDefaults(&heavy_ad_html);

  return heavy_ad_html;
}

}  // namespace heavy_ads
