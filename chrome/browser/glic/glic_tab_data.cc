// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_tab_data.h"

#include <optional>

#include "base/strings/utf_string_conversions.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/sessions/content/session_tab_helper.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

// FocusedTabCandidate Implementations:
FocusedTabCandidate::FocusedTabCandidate(
    content::WebContents* const web_contents,
    glic::mojom::InvalidCandidateError invalid_candidate_error)
    : focused_tab_candidate_contents(web_contents ? web_contents->GetWeakPtr()
                                                  : nullptr),
      invalid_candidate_error(invalid_candidate_error) {}
FocusedTabCandidate::~FocusedTabCandidate() = default;
FocusedTabCandidate::FocusedTabCandidate(const FocusedTabCandidate& other) =
    default;
FocusedTabCandidate::FocusedTabCandidate(FocusedTabCandidate&& other) noexcept
    : focused_tab_candidate_contents(
          std::move(other.focused_tab_candidate_contents)),
      invalid_candidate_error(std::move(other.invalid_candidate_error)) {}
FocusedTabCandidate& FocusedTabCandidate::operator=(
    const FocusedTabCandidate& other) {
  if (this != &other) {
    focused_tab_candidate_contents = other.focused_tab_candidate_contents;
    invalid_candidate_error = other.invalid_candidate_error;
  }
  return *this;
}
FocusedTabCandidate& FocusedTabCandidate::operator=(
    FocusedTabCandidate&& other) noexcept {
  if (this != &other) {
    focused_tab_candidate_contents =
        std::move(other.focused_tab_candidate_contents);
    invalid_candidate_error = std::move(other.invalid_candidate_error);
  }
  return *this;
}
bool FocusedTabCandidate::operator==(const FocusedTabCandidate& other) const {
  content::WebContents* this_contents_ptr =
      focused_tab_candidate_contents.get();
  content::WebContents* other_contents_ptr =
      other.focused_tab_candidate_contents.get();
  bool focused_tab_candidate_contents_changed =
      (focused_tab_candidate_contents.WasInvalidated() ||
       this_contents_ptr != other_contents_ptr);
  bool invalid_candidate_error_changed =
      invalid_candidate_error != other.invalid_candidate_error;
  return !focused_tab_candidate_contents_changed &&
         !invalid_candidate_error_changed;
}

// FocusedTabData Implementations:
FocusedTabData::FocusedTabData(
    content::WebContents* const web_contents,
    std::optional<glic::mojom::InvalidCandidateError> invalid_candidate_error,
    std::optional<glic::mojom::NoCandidateTabError> no_candidate_tab_error) {
  if (web_contents && !invalid_candidate_error) {
    focused_tab_contents = web_contents->GetWeakPtr();
  } else {
    if (web_contents && invalid_candidate_error) {
      focused_tab_candidate =
          FocusedTabCandidate(web_contents, invalid_candidate_error.value());
    } else {
      if (!no_candidate_tab_error) {
        no_candidate_tab_error = glic::mojom::NoCandidateTabError::kUnknown;
      }
      this->no_candidate_tab_error = std::move(*no_candidate_tab_error);
    }
  }
}
FocusedTabData::~FocusedTabData() = default;
FocusedTabData::FocusedTabData(const FocusedTabData& other) = default;
FocusedTabData::FocusedTabData(FocusedTabData&& other) noexcept
    : focused_tab_contents(std::move(other.focused_tab_contents)),
      focused_tab_candidate(std::move(other.focused_tab_candidate)),
      no_candidate_tab_error(std::move(other.no_candidate_tab_error)) {}
FocusedTabData& FocusedTabData::operator=(const FocusedTabData& other) {
  if (this != &other) {
    focused_tab_contents = other.focused_tab_contents;
    no_candidate_tab_error = other.no_candidate_tab_error;
    focused_tab_candidate = other.focused_tab_candidate;
  }
  return *this;
}
FocusedTabData& FocusedTabData::operator=(FocusedTabData&& other) noexcept {
  if (this != &other) {
    focused_tab_contents = std::move(other.focused_tab_contents);
    no_candidate_tab_error = std::move(other.no_candidate_tab_error);
    focused_tab_candidate = std::move(other.focused_tab_candidate);
  }
  return *this;
}
bool FocusedTabData::operator==(const FocusedTabData& other) const {
  content::WebContents* this_contents_ptr = focused_tab_contents.get();
  content::WebContents* other_contents_ptr = other.focused_tab_contents.get();
  bool focus_contents_changed = (focused_tab_contents.WasInvalidated() ||
                                 this_contents_ptr != other_contents_ptr);
  bool no_candidate_tab_error_changed =
      no_candidate_tab_error != other.no_candidate_tab_error;
  bool focused_tab_candidate_changed =
      focused_tab_candidate != other.focused_tab_candidate;
  return !focus_contents_changed && !no_candidate_tab_error_changed &&
         !focused_tab_candidate_changed;
}

// CreateTabData Implementation:
glic::mojom::TabDataPtr CreateTabData(content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  SkBitmap favicon;
  auto* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  if (favicon_driver) {
    if (favicon_driver->FaviconIsValid()) {
      favicon = favicon_driver->GetFavicon().AsBitmap();
    }
  }
  return glic::mojom::TabData::New(
      sessions::SessionTabHelper::IdForTab(web_contents).id(),
      sessions::SessionTabHelper::IdForWindowContainingTab(web_contents).id(),
      web_contents->GetLastCommittedURL(),
      base::UTF16ToUTF8(web_contents->GetTitle()), favicon,
      web_contents->GetContentsMimeType());
}

// CreateFocusedTabData Implementation:
glic::mojom::FocusedTabDataPtr CreateFocusedTabData(
    FocusedTabData focused_tab_data) {
  if (focused_tab_data.focused_tab_contents.get()) {
    return glic::mojom::FocusedTabData::NewFocusedTab(
        CreateTabData(focused_tab_data.focused_tab_contents.get()));
  }
  std::optional<FocusedTabCandidate> focused_tab_candidate =
      focused_tab_data.focused_tab_candidate;
  if (focused_tab_candidate.has_value()) {
    return glic::mojom::FocusedTabData::NewFocusedTabCandidate(
        glic::mojom::FocusedTabCandidate::New(
            CreateTabData(focused_tab_candidate.value()
                              .focused_tab_candidate_contents.get()),
            focused_tab_candidate.value().invalid_candidate_error));
  }

  return glic::mojom::FocusedTabData::NewNoCandidateTabError(
      focused_tab_data.no_candidate_tab_error.value());
}

}  // namespace glic
