// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/hung_renderer_infobar_delegate.h"

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "ui/base/l10n/l10n_util.h"

// static
void HungRendererInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    content::RenderProcessHost* render_process_host) {
  DCHECK(render_process_host);
  infobar_manager->AddInfoBar(std::make_unique<infobars::ConfirmInfoBar>(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new HungRendererInfoBarDelegate(render_process_host))));
}

HungRendererInfoBarDelegate::HungRendererInfoBarDelegate(
    content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host) {}

HungRendererInfoBarDelegate::~HungRendererInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
HungRendererInfoBarDelegate::GetIdentifier() const {
  return HUNG_RENDERER_INFOBAR_DELEGATE_ANDROID;
}

HungRendererInfoBarDelegate*
HungRendererInfoBarDelegate::AsHungRendererInfoBarDelegate() {
  return this;
}

int HungRendererInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_FROZEN_TAB;
}

std::u16string HungRendererInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_INFOBAR);
}

std::u16string HungRendererInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      (button == BUTTON_OK) ? IDS_BROWSER_HANGMONITOR_RENDERER_INFOBAR_END
                            : IDS_BROWSER_HANGMONITOR_RENDERER_WAIT);
}

bool HungRendererInfoBarDelegate::Accept() {
  render_process_host_->Shutdown(content::RESULT_CODE_HUNG);
  return true;
}

bool HungRendererInfoBarDelegate::Cancel() {
  return true;
}
