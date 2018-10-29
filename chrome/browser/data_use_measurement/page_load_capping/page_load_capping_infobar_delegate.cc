// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_infobar_delegate.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void RecordInteractionUMA(
    PageLoadCappingInfoBarDelegate::InfoBarInteraction interaction) {
  UMA_HISTOGRAM_ENUMERATION("HeavyPageCapping.InfoBarInteraction", interaction);
}

// The infobar that allows the user to resume resource loading on the page.
class ResumeDelegate : public PageLoadCappingInfoBarDelegate {
 public:
  // |pause_callback| will either pause subresource loading or resume it based
  // on the passed in bool.
  explicit ResumeDelegate(const PauseCallback& pause_callback)
      : pause_callback_(pause_callback) {
    DCHECK(!pause_callback_.is_null());
  }
  ~ResumeDelegate() override = default;

 private:
  // PageLoadCappingInfoBarDelegate:
  base::string16 GetMessageText() const override {
    return l10n_util::GetStringUTF16(IDS_PAGE_CAPPING_STOPPED_TITLE);
  }
  base::string16 GetLinkText() const override {
    return l10n_util::GetStringUTF16(IDS_PAGE_CAPPING_CONTINUE_MESSAGE);
  }
  bool LinkClicked(WindowOpenDisposition disposition) override {
    RecordInteractionUMA(InfoBarInteraction::kResumedPage);
    // Pass false to resume subresource loading.
    pause_callback_.Run(false);
    return true;
  }

  // |pause_callback| will either pause subresource loading or resume it based
  // on the passed in bool.
  const PauseCallback pause_callback_;

  DISALLOW_COPY_AND_ASSIGN(ResumeDelegate);
};

// The infobar that allows the user to pause resoruce loading on the page.
class PauseDelegate : public PageLoadCappingInfoBarDelegate {
 public:
  // This object is destroyed when the page is terminated, and methods related
  // to functionality of the InfoBar (E.g., LinkClicked()), are not called from
  // page destructors. This object is also destroyed on all non-same page
  // navigations.
  // |pause_callback| is a callback that will pause subresource loading on the
  // page.
  // |time_to_expire_callback| is used to get the earliest time at which the
  // page is considered to have stopped using data.
  explicit PauseDelegate(const PauseCallback& pause_callback,
                         const TimeToExpireCallback& time_to_expire_callback)
      : pause_callback_(pause_callback),
        time_to_expire_callback_(time_to_expire_callback),
        weak_factory_(this) {
    // When creating the InfoBar, it should not already be expired.
    DCHECK(!time_to_expire_callback_.is_null());
    DCHECK(!pause_callback_.is_null());
    base::TimeDelta time_to_expire;
    time_to_expire_callback_.Run(&time_to_expire);
    RunDelayedCheck(time_to_expire);
  }
  ~PauseDelegate() override = default;

 private:
  // PageLoadCappingInfoBarDelegate:
  base::string16 GetMessageText() const override {
    return l10n_util::GetStringUTF16(IDS_PAGE_CAPPING_TITLE);
  }

  base::string16 GetLinkText() const override {
    return l10n_util::GetStringUTF16(IDS_PAGE_CAPPING_STOP_MESSAGE);
  }

  bool LinkClicked(WindowOpenDisposition disposition) override {
    RecordInteractionUMA(InfoBarInteraction::kPausedPage);

      // Pause subresouce loading on the page.
      pause_callback_.Run(true);

    auto* infobar_manager = infobar()->owner();
    // |this| will be gone after this call.
    infobar_manager->ReplaceInfoBar(
        infobar(), infobar_manager->CreateConfirmInfoBar(
                       std::make_unique<ResumeDelegate>(pause_callback_)));

    return false;
  }

  void RunDelayedCheck(base::TimeDelta time_to_expire) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PauseDelegate::ExpireIfNecessary,
                       weak_factory_.GetWeakPtr()),
        time_to_expire);
  }

  void ExpireIfNecessary() {
    base::TimeDelta time_to_expire;
    time_to_expire_callback_.Run(&time_to_expire);

    // When the owner of |time_to_expire_callback_| is deleted, or it returns a
    // TimeDelta of 0, the InfoBar should be deleted. Otherwise, re-evaluate
    // after |time_to_expire|.
    if (time_to_expire > base::TimeDelta()) {
      RunDelayedCheck(time_to_expire);
      return;
    }

    RecordInteractionUMA(InfoBarInteraction::kDismissedByNetworkStopped);

    // |this| will be gone after this call.
    infobar()->RemoveSelf();
  }

  // |pause_callback| will either pause subresource loading or resume it based
  // on the passed in bool.
  const PauseCallback pause_callback_;

  // Used to get the earliest time at which the page is considered to have
  // stopped using data.
  const TimeToExpireCallback time_to_expire_callback_;

  base::WeakPtrFactory<PauseDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PauseDelegate);
};

}  // namespace

// static
bool PageLoadCappingInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const PauseCallback& pause_callback,
    const TimeToExpireCallback& time_to_expire_callback) {
  auto* infobar_service = InfoBarService::FromWebContents(web_contents);
  RecordInteractionUMA(InfoBarInteraction::kShowedInfoBar);
  // WrapUnique is used to allow for a private constructor.
  return infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(std::make_unique<PauseDelegate>(
          pause_callback, time_to_expire_callback)));
}

PageLoadCappingInfoBarDelegate::~PageLoadCappingInfoBarDelegate() = default;

PageLoadCappingInfoBarDelegate::PageLoadCappingInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
PageLoadCappingInfoBarDelegate::GetIdentifier() const {
  return PAGE_LOAD_CAPPING_INFOBAR_DELEGATE;
}

int PageLoadCappingInfoBarDelegate::GetIconId() const {
// TODO(ryansturm): Make data saver resources available on other platforms.
// https://crbug.com/820594
#if defined(OS_ANDROID)
  return IDR_ANDROID_INFOBAR_PREVIEWS;
#else
  return kNoIconID;
#endif
}

bool PageLoadCappingInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return details.is_navigation_to_different_page;
}

int PageLoadCappingInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
