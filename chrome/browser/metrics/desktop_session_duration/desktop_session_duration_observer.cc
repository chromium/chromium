// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_view_host.h"

namespace metrics {

DesktopSessionDurationObserver::DesktopSessionDurationObserver(
    content::WebContents* web_contents,
    DesktopSessionDurationTracker* service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DesktopSessionDurationObserver>(
          *web_contents),
      service_(service) {
  RegisterInputEventObserver(web_contents->GetPrimaryMainFrame());
}

DesktopSessionDurationObserver::~DesktopSessionDurationObserver() {}

// static
DesktopSessionDurationObserver*
DesktopSessionDurationObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  if (!DesktopSessionDurationTracker::IsInitialized())
    return nullptr;

  DesktopSessionDurationObserver* observer = FromWebContents(web_contents);
  if (!observer) {
    observer = new DesktopSessionDurationObserver(
        web_contents, DesktopSessionDurationTracker::Get());
    web_contents->SetUserData(UserDataKey(), base::WrapUnique(observer));
  }
  return observer;
}

void DesktopSessionDurationObserver::RegisterInputEventObserver(
    content::RenderFrameHost* host) {
  if (host != nullptr)
    host->GetRenderWidgetHost()->AddInputEventObserver(this);
}

void DesktopSessionDurationObserver::UnregisterInputEventObserver(
    content::RenderFrameHost* host) {
  if (host != nullptr)
    host->GetRenderWidgetHost()->RemoveInputEventObserver(this);
}

void DesktopSessionDurationObserver::OnInputEvent(
    const blink::WebInputEvent& event) {
  service_->OnUserEvent();
}

void DesktopSessionDurationObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (!new_host->IsInPrimaryMainFrame())
    return;

  UnregisterInputEventObserver(old_host);
  RegisterInputEventObserver(new_host);

  if (new_host->GetSiteInstance()->GetSiteURL().host() ==
      old_host->GetSiteInstance()->GetSiteURL().host()) {
    return;
  }

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(Profile::FromBrowserContext(
          new_host->GetSiteInstance()->GetBrowserContext()));
  CHECK(template_url_service);

  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();

  if (default_search_engine &&
      new_host->GetSiteInstance()->GetSiteURL().host() ==
          GURL(default_search_engine->url()).host()) {
    service_->IncrementDefaultSearchCounter();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DesktopSessionDurationObserver);

}  // namespace metrics
