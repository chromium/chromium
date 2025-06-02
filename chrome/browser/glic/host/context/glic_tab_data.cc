// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_tab_data.h"

#include <optional>
#include <utility>

#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace glic {

TabDataObserver::TabDataObserver(
    content::WebContents* web_contents,
    bool observe_current_page_only,
    base::RepeatingCallback<void(glic::mojom::TabDataPtr)> tab_data_changed)
    : content::WebContentsObserver(web_contents),
      observe_current_page_only_(observe_current_page_only),
      tab_data_changed_(std::move(tab_data_changed)) {
  if (web_contents) {
    auto* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents);
    if (favicon_driver) {
      favicon_driver->AddObserver(this);
    }
  }
}

TabDataObserver::~TabDataObserver() {
  ClearObservation();
}

void TabDataObserver::ClearObservation() {
  // If the web contents is destroyed, web_contents() returns null. The favicon
  // driver is owned by the web contents, so it's not necessary to remove our
  // observer if the web contents is destroyed.
  // Note, we do not used a scoped observation because there is no event
  // notifying us when a web contents is destroyed.
  if (web_contents()) {
    auto* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents());
    if (favicon_driver) {
      favicon_driver->RemoveObserver(this);
    }
  }
  Observe(nullptr);
}

void TabDataObserver::PrimaryPageChanged(content::Page& page) {
  if (observe_current_page_only_) {
    ClearObservation();
  } else {
    SendUpdate();
  }
}

void TabDataObserver::TitleWasSetForMainFrame(
    content::RenderFrameHost* render_frame_host) {
  SendUpdate();
}

void TabDataObserver::SendUpdate() {
  tab_data_changed_.Run(CreateTabData(web_contents()));
}

void TabDataObserver::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  SendUpdate();
}

int GetTabId(content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (tab) {
    return tab->GetHandle().raw_value();
  } else {
    return tabs::TabHandle::Null().raw_value();
  }
}

const GURL& GetTabUrl(content::WebContents* web_contents) {
  return web_contents->GetLastCommittedURL();
}

// CreateTabData Implementation:
glic::mojom::TabDataPtr CreateTabData(content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  SkBitmap favicon;
  auto* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  if (favicon_driver && favicon_driver->FaviconIsValid()) {
    // Attempt to get a 32x32 favicon by default (16x16 DIP at 2x scale).
    favicon = favicon_driver->GetFavicon()
                  .ToImageSkia()
                  ->GetRepresentation(2.0f)
                  .GetBitmap();
  }
  return glic::mojom::TabData::New(
      GetTabId(web_contents),
      sessions::SessionTabHelper::IdForWindowContainingTab(web_contents).id(),
      GetTabUrl(web_contents), base::UTF16ToUTF8(web_contents->GetTitle()),
      favicon, web_contents->GetContentsMimeType());
}

// CreateFocusedTabData Implementation:
glic::mojom::FocusedTabDataPtr CreateFocusedTabData(
    const FocusedTabData& focused_tab_data) {
  if (focused_tab_data.is_focus()) {
    return mojom::FocusedTabData::NewFocusedTab(
        CreateTabData(focused_tab_data.focus()->GetContents()));
  }
  return mojom::FocusedTabData::NewNoFocusedTabData(
      mojom::NoFocusedTabData::New(
          CreateTabData(focused_tab_data.unfocused_tab()
                            ? focused_tab_data.unfocused_tab()->GetContents()
                            : nullptr),
          focused_tab_data.GetFocus().error()));
}

FocusedTabData::FocusedTabData(tabs::TabInterface* tab) : data_(tab) {}
FocusedTabData::FocusedTabData(const std::string& error,
                               tabs::TabInterface* unfocused_tab)
    : data_(error), unfocused_tab_(unfocused_tab) {}
FocusedTabData::~FocusedTabData() = default;

base::expected<tabs::TabInterface*, std::string> FocusedTabData::GetFocus()
    const {
  if (is_focus()) {
    return focus();
  }
  return base::unexpected(std::get<1>(data_));
}

}  // namespace glic
