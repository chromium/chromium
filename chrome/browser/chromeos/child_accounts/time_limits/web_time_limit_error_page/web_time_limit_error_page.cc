// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_error_page/web_time_limit_error_page.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

std::u16string GetTimeLimitMessage(base::TimeDelta time_limit) {
  return ui::TimeFormat::Detailed(ui::TimeFormat::Format::FORMAT_DURATION,
                                  ui::TimeFormat::Length::LENGTH_LONG,
                                  /* cutoff */ 3, time_limit);
}

std::string GetWebTimeLimitErrorPage(
    std::u16string block_header,
    std::u16string block_message,
    base::TimeDelta time_limit,
    const std::string& app_locale,
    const base::Optional<std::u16string>& title) {
  base::DictionaryValue strings;

  if (!title.has_value()) {
    strings.SetString("blockPageTitle",
                      l10n_util::GetStringFUTF16(
                          IDS_WEB_TIME_LIMIT_ERROR_PAGE_TITLE,
                          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  } else {
    strings.SetString("blockPageTitle", title.value());
  }

  strings.SetString("blockPageHeader", block_header);
  strings.SetString(
      "blockPageMessage",
      base::StrCat({block_message, u" ",
                    l10n_util::GetStringFUTF16(
                        IDS_WEB_TIME_LIMIT_ERROR_PAGE_NEXT_ACCESS_TIME,
                        GetTimeLimitMessage(time_limit))}));

  webui::SetLoadTimeDataDefaults(app_locale, &strings);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_WEB_TIME_LIMIT_ERROR_PAGE_HTML);
  webui::AppendWebUiCssTextDefaults(&html);
  std::string error_html = webui::GetI18nTemplateHtml(html, &strings);
  return error_html;
}

}  // namespace

std::string GetWebTimeLimitChromeErrorPage(
    const std::string& domain,
    const base::Optional<std::u16string>& title,
    base::TimeDelta time_limit,
    const std::string& app_locale) {
  auto block_header = l10n_util::GetStringFUTF16(
      IDS_WEB_TIME_LIMIT_ERROR_PAGE_CHROME_HEADER, base::UTF8ToUTF16(domain));

  auto block_message = l10n_util::GetStringFUTF16(
      IDS_WEB_TIME_LIMIT_ERROR_PAGE_CHROME_MESSAGE,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));

  return GetWebTimeLimitErrorPage(block_header, block_message, time_limit,
                                  app_locale, title);
}

std::string GetWebTimeLimitAppErrorPage(base::TimeDelta time_limit,
                                        const std::string& app_locale,
                                        const std::string& app_name) {
  auto block_header = l10n_util::GetStringFUTF16(
      IDS_WEB_TIME_LIMIT_ERROR_PAGE_APP_HEADER,
      base::UTF8ToUTF16(base::StringPiece(app_name)));

  auto block_message = l10n_util::GetStringFUTF16(
      IDS_WEB_TIME_LIMIT_ERROR_PAGE_APP_MESSAGE,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));

  return GetWebTimeLimitErrorPage(block_header, block_message, time_limit,
                                  app_locale, base::UTF8ToUTF16(app_name));
}
