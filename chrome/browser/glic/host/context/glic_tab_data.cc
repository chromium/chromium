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
namespace {
// Returns whether `a` and `b` both point to the same web contents.
// Note that if both `a` and `b` are invalidated, this returns true, even if the
// web contents they once pointed to is different. For our purposes, this is OK.
bool IsWeakWebContentsEqual(base::WeakPtr<content::WebContents> a,
                            base::WeakPtr<content::WebContents> b) {
  return std::make_pair(a.get(), a.WasInvalidated()) ==
         std::make_pair(b.get(), b.WasInvalidated());
}
}  // namespace

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

NoFocusedTabData::NoFocusedTabData() = default;
NoFocusedTabData::NoFocusedTabData(std::string_view reason,
                                   content::WebContents* tab)
    : active_tab(tab ? tab->GetWeakPtr() : nullptr), no_focus_reason(reason) {}
NoFocusedTabData::~NoFocusedTabData() = default;
NoFocusedTabData::NoFocusedTabData(const NoFocusedTabData& other) = default;
NoFocusedTabData& NoFocusedTabData::operator=(const NoFocusedTabData& other) =
    default;

base::expected<content::WebContents*, std::string_view>
FocusedTabData::GetFocus() const {
  using ResultType = decltype(GetFocus());
  return std::visit(
      base::Overloaded{
          [](const base::WeakPtr<content::WebContents>& focused_tab)
              -> ResultType {
            if (focused_tab) {
              return focused_tab.get();
            }
            return base::unexpected(std::string_view("focused tab removed"));
          },
          [](const NoFocusedTabData& no_focus) -> ResultType {
            return base::unexpected(std::string_view(no_focus.no_focus_reason));
          },
      },
      *this);
}

int GetTabId(content::WebContents* web_contents) {
  return sessions::SessionTabHelper::IdForTab(web_contents).id();
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
  return std::visit(
      base::Overloaded{
          [](const base::WeakPtr<content::WebContents>& focused_tab) {
            return mojom::FocusedTabData::NewFocusedTab(
                CreateTabData(focused_tab.get()));
          },
          [](const NoFocusedTabData& no_focus) {
            return mojom::FocusedTabData::NewNoFocusedTabData(
                mojom::NoFocusedTabData::New(
                    CreateTabData(no_focus.active_tab.get()),
                    std::string(no_focus.no_focus_reason)));
          },
      },
      focused_tab_data);
}

bool FocusedTabData::IsSame(const FocusedTabData& new_data) const {
  if (index() != new_data.index()) {
    return false;
  }
  switch (index()) {
    case 0:
      return IsWeakWebContentsEqual(std::get<0>(*this), std::get<0>(new_data));
    case 1:
      return std::get<1>(*this).IsSame(std::get<1>(new_data));
  }
  NOTREACHED();
}

bool NoFocusedTabData::IsSame(const NoFocusedTabData& other) const {
  return IsWeakWebContentsEqual(active_tab, other.active_tab) &&
         no_focus_reason == other.no_focus_reason;
}

}  // namespace glic
