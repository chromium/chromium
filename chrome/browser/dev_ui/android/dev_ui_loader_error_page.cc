// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dev_ui/android/dev_ui_loader_error_page.h"

#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/strings/grit/ui_strings.h"

namespace dev_ui {

std::string BuildErrorPageHtml() {
  ui::TemplateReplacements replacements;
  replacements["h1"] =
      l10n_util::GetStringUTF8(IDS_DEV_UI_LOADER_ERROR_HEADING);
  replacements["p"] =
      l10n_util::GetStringUTF8(IDS_ERRORPAGES_SUGGESTION_LIST_HEADER);
  replacements["li-1"] =
      l10n_util::GetStringUTF8(IDS_DEV_UI_LOADER_ERROR_SUGGEST_RELOAD);
  replacements["li-2"] =
      l10n_util::GetStringUTF8(IDS_DEV_UI_LOADER_ERROR_SUGGEST_CHECK_INTERNET);

  std::string source =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_DEV_UI_LOADER_ERROR_HTML);
  return ui::ReplaceTemplateExpressions(source, replacements);
}

}  // namespace dev_ui
