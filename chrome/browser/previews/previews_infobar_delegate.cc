// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_delegate.h"

#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/infobars/core/infobar.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_logger.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/infobars/previews_infobar.h"
#endif

namespace {

static const char kPreviewInfobarEventType[] = "InfoBar";

void RecordPreviewsInfoBarAction(
    previews::PreviewsType previews_type,
    PreviewsInfoBarDelegate::PreviewsInfoBarAction action) {
  int32_t max_limit =
      static_cast<int32_t>(PreviewsInfoBarDelegate::INFOBAR_INDEX_BOUNDARY);
  base::LinearHistogram::FactoryGet(
      base::StringPrintf("Previews.InfoBarAction.%s",
                         GetStringNameForType(previews_type).c_str()),
      1, max_limit, max_limit + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(static_cast<int32_t>(action));
}

}  // namespace

PreviewsInfoBarDelegate::~PreviewsInfoBarDelegate() {
  RecordPreviewsInfoBarAction(previews_type_, infobar_dismissed_action_);
}

// static
void PreviewsInfoBarDelegate::Create(
    content::WebContents* web_contents,
    previews::PreviewsType previews_type,
    bool is_data_saver_user,
    previews::PreviewsUIService* previews_ui_service) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);

  // The WebContents may not have TabHelpers set. If TabHelpers are not set,
  // don't show Previews infobars.
  if (!ui_tab_helper || !infobar_service)
    return;
  if (ui_tab_helper->displayed_preview_ui())
    return;

  std::unique_ptr<PreviewsInfoBarDelegate> delegate(new PreviewsInfoBarDelegate(
      ui_tab_helper, previews_type, is_data_saver_user));

#if defined(OS_ANDROID)
  std::unique_ptr<infobars::InfoBar> infobar_ptr(
      PreviewsInfoBar::CreateInfoBar(infobar_service, std::move(delegate)));
#else
  std::unique_ptr<infobars::InfoBar> infobar_ptr(
      infobar_service->CreateConfirmInfoBar(std::move(delegate)));
#endif

  infobar_service->AddInfoBar(std::move(infobar_ptr));
  uint64_t page_id = (ui_tab_helper->previews_user_data())
                         ? ui_tab_helper->previews_user_data()->page_id()
                         : 0;

  if (previews_ui_service) {
    // Not in incognito mode or guest mode.
    previews_ui_service->previews_logger()->LogMessage(
        kPreviewInfobarEventType,
        previews::GetDescriptionForInfoBarDescription(previews_type),
        web_contents->GetController()
            .GetLastCommittedEntry()
            ->GetRedirectChain()[0] /* GURL */,
        base::Time::Now(), page_id);
  }

  RecordPreviewsInfoBarAction(previews_type, INFOBAR_SHOWN);
  ui_tab_helper->set_displayed_preview_ui(true);
}

PreviewsInfoBarDelegate::PreviewsInfoBarDelegate(
    PreviewsUITabHelper* ui_tab_helper,
    previews::PreviewsType previews_type,
    bool is_data_saver_user)
    : ConfirmInfoBarDelegate(),
      ui_tab_helper_(ui_tab_helper),
      previews_type_(previews_type),
      infobar_dismissed_action_(INFOBAR_DISMISSED_BY_TAB_CLOSURE),
      message_text_(l10n_util::GetStringUTF16(
          is_data_saver_user ? IDS_PREVIEWS_INFOBAR_SAVED_DATA_TITLE
                             : IDS_PREVIEWS_INFOBAR_FASTER_PAGE_TITLE)) {
  DCHECK(previews_type_ != previews::PreviewsType::NONE &&
         previews_type_ != previews::PreviewsType::UNSPECIFIED);
}

infobars::InfoBarDelegate::InfoBarIdentifier
PreviewsInfoBarDelegate::GetIdentifier() const {
  return DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE;
}

int PreviewsInfoBarDelegate::GetIconId() const {
#if defined(OS_ANDROID)
  return IDR_ANDROID_INFOBAR_PREVIEWS;
#else
  return kNoIconID;
#endif
}

bool PreviewsInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  infobar_dismissed_action_ = details.is_reload
                                  ? INFOBAR_DISMISSED_BY_RELOAD
                                  : INFOBAR_DISMISSED_BY_NAVIGATION;
  return InfoBarDelegate::ShouldExpire(details);
}

void PreviewsInfoBarDelegate::InfoBarDismissed() {
  infobar_dismissed_action_ = INFOBAR_DISMISSED_BY_USER;
}

base::string16 PreviewsInfoBarDelegate::GetMessageText() const {
// Android has a custom infobar that calls GetStalePreviewTimestampText() and
// adds the timestamp in a separate description view. Other OS's can enable
// previews for debugging purposes and don't have a custom infobar with a
// description view, so the timestamp should be appended to the message.
#if defined(OS_ANDROID)
  return message_text_;
#else
  base::string16 timestamp = GetStalePreviewTimestampText();
  if (timestamp.empty())
    return message_text_;
  // This string concatenation wouldn't fly for l10n, but this is only a hack
  // for Chromium devs and not expected to ever appear for users.
  return message_text_ + base::ASCIIToUTF16(" - ") + timestamp;
#endif
}

int PreviewsInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 PreviewsInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_PREVIEWS_INFOBAR_LINK);
}

bool PreviewsInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  infobar_dismissed_action_ = INFOBAR_LOAD_ORIGINAL_CLICKED;

  ui_tab_helper_->ReloadWithoutPreviews(previews_type_);

  return true;
}

base::string16 PreviewsInfoBarDelegate::GetStalePreviewTimestampText() const {
  if (!ui_tab_helper_)
    return base::string16();

  const base::string16 text = ui_tab_helper_->GetStalePreviewTimestampText();
  if (text.length() > 0)
    ui_tab_helper_->set_displayed_preview_timestamp(true);
  return text;
}
