// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/hung_renderer_infobar_delegate.h"

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "ui/base/l10n/l10n_util.h"

// static
void HungRendererInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    content::RenderProcessHost* render_process_host) {
  DCHECK(render_process_host);
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new HungRendererInfoBarDelegate(render_process_host))));
}

void HungRendererInfoBarDelegate::OnRendererResponsive() {
  LogEvent(RENDERER_BECAME_RESPONSIVE);
}

HungRendererInfoBarDelegate::HungRendererInfoBarDelegate(
    content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host),
      terminal_event_logged_for_uma_(false) {}

HungRendererInfoBarDelegate::~HungRendererInfoBarDelegate() {
  if (!terminal_event_logged_for_uma_)
    LogEvent(TAB_CLOSED);
}

infobars::InfoBarDelegate::InfoBarIdentifier
HungRendererInfoBarDelegate::GetIdentifier() const {
  return HUNG_RENDERER_INFOBAR_DELEGATE_ANDROID;
}

void HungRendererInfoBarDelegate::InfoBarDismissed() {
  LogEvent(CLOSE_CLICKED);
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
  LogEvent(KILL_CLICKED);
  render_process_host_->Shutdown(content::RESULT_CODE_HUNG);
  return true;
}

bool HungRendererInfoBarDelegate::Cancel() {
  LogEvent(WAIT_CLICKED);
  return true;
}

void HungRendererInfoBarDelegate::LogEvent(Event event) {
  DCHECK(!terminal_event_logged_for_uma_);
  terminal_event_logged_for_uma_ = true;
  UMA_HISTOGRAM_ENUMERATION("Renderer.Hung.MobileInfoBar.UserEvent", event,
                            EVENT_COUNT);
}
