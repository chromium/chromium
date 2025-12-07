// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_tab_data.h"

#include <cstdint>
#include <optional>
#include <utility>
// #include <cstring>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/common/chrome_features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/codec_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace glic {

namespace {
// Rate limit tab updates if more than this many have happened since the last
// (cross-document) navigation. This is > 0 so that we propagate changes rapidly
// for pages that do not have strange behavior of updating too often.
constexpr size_t kMaxTabUpdatesBeforeRateLimiting = 4;

bool IsForeground(content::Visibility visibility) {
  return visibility != content::Visibility::HIDDEN;
}

}  // namespace

// For testing and debugging.
std::string TabDataDebugString(const mojom::TabData& tab_data) {
  auto dict = base::DictValue()
                  .Set("tab_id", tab_data.tab_id)
                  .Set("url", tab_data.url.spec())
                  .Set("document_mime_type", tab_data.document_mime_type);
  if (tab_data.title) {
    dict.Set("title", *tab_data.title);
  }
  if (tab_data.is_media_active) {
    dict.Set("is_media_active", *tab_data.is_media_active);
  }
  if (tab_data.is_observable) {
    dict.Set("is_observable", *tab_data.is_observable);
  }
  return dict.DebugString();
}

TabDataChange::TabDataChange() = default;
TabDataChange::TabDataChange(TabDataChangeCauseSet causes,
                             glic::mojom::TabDataPtr tab_data)
    : causes(causes), tab_data(std::move(tab_data)) {}
TabDataChange::~TabDataChange() = default;
TabDataChange::TabDataChange(TabDataChange&& src) = default;
TabDataChange& TabDataChange::operator=(TabDataChange&& src) = default;
std::ostream& operator<<(std::ostream& os, const TabDataChangeCause& cause) {
  switch (cause) {
    case TabDataChangeCause::kFavicon:
      return os << "Favicon";
    case TabDataChangeCause::kTitle:
      return os << "Title";
    case TabDataChangeCause::kAudioState:
      return os << "AudioState";
    case TabDataChangeCause::kVisibility:
      return os << "Visibility";
    case TabDataChangeCause::kSameDocNavigation:
      return os << "SameDocNavigation";
    case TabDataChangeCause::kCrossDocNavigation:
      return os << "CrossDocNavigation";
    case TabDataChangeCause::kTabChanged:
      return os << "TabChanged";
  }
}
std::ostream& operator<<(std::ostream& os,
                         const TabDataChangeCauseSet& causes) {
  os << "Causes(";
  for (auto cause : causes) {
    os << cause << ", ";
  }
  return os << ")";
}

std::ostream& operator<<(std::ostream& os, const TabDataChange& change) {
  os << "(" << change.causes << ", ";
  if (change.tab_data) {
    os << TabDataDebugString(*change.tab_data);
  } else {
    os << "null TabDataPtr";
  }
  return os << ")";
}

TabDataObserver::TabDataObserver(
    content::WebContents* web_contents,
    base::RepeatingCallback<void(TabDataChange)> tab_data_changed)
    : TabDataObserver(web_contents
                          ? tabs::TabInterface::GetFromContents(web_contents)
                          : nullptr,
                      web_contents,
                      std::move(tab_data_changed)) {}

TabDataObserver::TabDataObserver(
    tabs::TabInterface* tab,
    content::WebContents* web_contents,
    base::RepeatingCallback<void(TabDataChange)> tab_data_changed)
    : content::WebContentsObserver(web_contents),
      tab_data_changed_(std::move(tab_data_changed)) {
  if (web_contents) {
    auto* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents);
    if (favicon_driver) {
      favicon_driver->AddObserver(this);
    }
    tab_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
        &TabDataObserver::OnTabWillDetach, base::Unretained(this)));
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
  deferred_update_.Stop();
  ReportUpdatesPerNavigation();
  updates_since_navigation_ = 0;
  tab_detach_subscription_ = {};
}

void TabDataObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  if (!navigation_handle->IsSameDocument()) {
    ReportUpdatesPerNavigation();
    change_causes_.Put(TabDataChangeCause::kCrossDocNavigation);
    SendUpdate();
    updates_since_navigation_ = 0;
    return;
  }

  // Same document navigations can be made rapidly from javascript.
  change_causes_.Put(TabDataChangeCause::kSameDocNavigation);
  SendRateLimitedUpdate();
}

void TabDataObserver::ReportUpdatesPerNavigation() {
  if (updates_since_navigation_ == 0) {
    return;
  }
  base::UmaHistogramCounts100("Glic.Api.TabData.UpdatesPerNavigation",
                              updates_since_navigation_);
}

void TabDataObserver::TitleWasSetForMainFrame(
    content::RenderFrameHost* render_frame_host) {
  // Title changes can be made rapidly from javascript.
  change_causes_.Put(TabDataChangeCause::kTitle);
  SendRateLimitedUpdate();
}

void TabDataObserver::SendRateLimitedUpdate() {
  if (updates_since_navigation_ < kMaxTabUpdatesBeforeRateLimiting) {
    SendUpdate();
    return;
  }
  if (!deferred_update_.IsRunning()) {
    deferred_update_.Start(
        FROM_HERE, base::Milliseconds(250),
        base::BindOnce(&TabDataObserver::SendUpdate, base::Unretained(this)));
  }
}

void TabDataObserver::SendUpdate() {
  deferred_update_.Stop();
  ++updates_since_navigation_;
  tab_data_changed_.Run({change_causes_, CreateTabData(web_contents())});
  change_causes_ = {};
}

void TabDataObserver::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  change_causes_.Put(TabDataChangeCause::kFavicon);
  SendUpdate();
}

void TabDataObserver::OnTabWillDetach(tabs::TabInterface* tab,
                                      tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    ClearObservation();
  }
}

void TabDataObserver::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  deferred_update_.SetTaskRunner(std::move(task_runner));
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
  std::optional<GURL> favicon_url;
  if (favicon_driver && favicon_driver->FaviconIsValid()) {
    // Attempt to get a 32x32 favicon by default (16x16 DIP at 2x scale).
    favicon = favicon_driver->GetFavicon()
                  .ToImageSkia()
                  ->GetRepresentation(2.0f)
                  .GetBitmap();
    if (base::FeatureList::IsEnabled(features::kGlicFaviconDataUrls)) {
      favicon_url = GURL(skia::EncodePngAsDataUri(favicon.pixmap()));
    }
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);

  // TODO(b/426644734): investigate triggering updates due to changes to
  // observability for focused tab data.
  bool is_audible = web_contents->IsCurrentlyAudible();
  bool is_tab_content_captured = web_contents->IsBeingCaptured();
  bool is_foreground = IsForeground(web_contents->GetVisibility());
  bool is_observable = is_audible || is_foreground;
  bool is_active_in_window = false;
  bool is_window_active = false;
  if (base::FeatureList::IsEnabled(features::kGlicGetTabByIdApi)) {
    is_active_in_window = tab && tab->IsActivated();
    // This code may be reached during the dragging of the tab out into a new
    // window. In that case the BrowserWindowInterface would be null, but we
    // cannot call GetBrowserWindowInterface to check for null. So we resort to
    // null checking the underlying tab strip.
    // TODO(crbug.com/456445100): Determine a better way to safely call this.
    is_window_active = tab &&
                       static_cast<tabs::TabModel*>(tab)->owning_model() &&
                       tab->GetBrowserWindowInterface()->IsActive();
  }
  return glic::mojom::TabData::New(
      GetTabId(web_contents),
      sessions::SessionTabHelper::IdForWindowContainingTab(web_contents).id(),
      GetTabUrl(web_contents), base::UTF16ToUTF8(web_contents->GetTitle()),
      favicon, favicon_url, web_contents->GetContentsMimeType(), is_observable,
      is_audible, is_tab_content_captured, is_active_in_window,
      is_window_active);
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

bool FaviconEquals(const ::SkBitmap& a, const ::SkBitmap& b) {
  if (&a == &b) {
    return true;
  }
  // Compare image properties.
  if (a.info() != b.info()) {
    return false;
  }
  // Compare image pixels.
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      if (a.getColor(x, y) != b.getColor(x, y)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace glic
