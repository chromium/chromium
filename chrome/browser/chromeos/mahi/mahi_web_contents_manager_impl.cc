// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/mahi_content_extraction_delegate.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_browser_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "components/pdf/browser/pdf_frame_util.h"
#include "components/pdf/common/constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "pdf/pdf_features.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

#if DCHECK_IS_ON()
#include "base/functional/callback_helpers.h"
#include "chromeos/constants/chromeos_features.h"
#endif

namespace mahi {

namespace {

using chromeos::mahi::ButtonType;
using chromeos::mahi::GetContentCallback;

// The character count threshold for a distillable page.
static constexpr int kCharCountThreshold = 300;

// Whether the `window` is a media app window.
bool IsMediaAppWindow(const aura::Window* window) {
  if (chromeos::MahiMediaAppContentManager::Get() &&
      chromeos::MahiMediaAppContentManager::Get()->ObservingWindow(window)) {
    return true;
  }

  return false;
}

crosapi::mojom::MahiPageInfoPtr ConvertWebContentStateToPageInfo(
    const WebContentState& web_content_state) {
  // Generates `page_info` from `web_content_state`.
  crosapi::mojom::MahiPageInfoPtr page_info = crosapi::mojom::MahiPageInfo::New(
      /*client_id, deprecated*/ base::UnguessableToken::Create(),
      /*page_id=*/web_content_state.page_id,
      /*url=*/web_content_state.url, /*title=*/web_content_state.title,
      /*favicon_image=*/web_content_state.favicon.DeepCopy(),
      /*is_distillable=*/std::nullopt,
      /*is_incognito=*/web_content_state.is_incognito);
  if (web_content_state.is_distillable.has_value()) {
    page_info->IsDistillable = web_content_state.is_distillable.value();
  }

  return page_info;
}

// Checks if |web_contents| contains a PDF
bool IsPDFWebContents(content::WebContents* web_contents) {
  return web_contents->GetContentsMimeType() == pdf::kPDFMimeType;
}

// Check if |web_contents| is from a incognito profile
bool IsFromIncognito(content::WebContents* web_contents) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return false;
  }

  return profile->IsIncognitoProfile();
}

// Get the RenderFrameHost that contains the PDF content.
content::RenderFrameHost* GetPDFRenderFrameHost(
    content::WebContents* contents) {
  // Pick the plugin frame host if `contents` is a PDF viewer guest. If using
  // OOPIF PDF viewer, pick the PDF extension frame host.
  content::RenderFrameHost* full_page_pdf_embedder_host =
      base::FeatureList::IsEnabled(chrome_pdf::features::kPdfOopif)
          ? pdf_frame_util::FindFullPagePdfExtensionHost(contents)
          : printing::GetFullPagePlugin(contents);
  content::RenderFrameHost* pdf_rfh = pdf_frame_util::FindPdfChildFrame(
      full_page_pdf_embedder_host ? full_page_pdf_embedder_host
                                  : contents->GetPrimaryMainFrame());
  return pdf_rfh;
}

// When the size of the AXTreeUpdate meets this threshold, we consider them
// contain enough content and start extraction without subsequence updates.
constexpr int kAXTreeUpdateByteSizeThreshold = 2000;

// When total observation time for accessibility changes for PDF greater than
// this limit, we stop observing the changes, and processes whatever updates
// received so far.
constexpr base::TimeDelta kPdfObservationTimeLimit = base::Seconds(30);

}  // namespace

MahiPDFObserver::MahiPDFObserver(content::WebContents* web_contents,
                                 ui::AXMode accessibility_mode,
                                 ui::AXTreeID tree_id,
                                 PDFContentObservedCallback callback)
    : tree_id_(tree_id), callback_(std::move(callback)) {
  Observe(web_contents);

  timer_.Start(FROM_HERE, kPdfObservationTimeLimit,
               base::BindOnce(&MahiPDFObserver::OnTimerFired,
                              weak_ptr_factory_.GetWeakPtr()));

  // Enable accessibility for the top level render frame and all descendants.
  // This causes AXTreeSerializer to reset and send accessibility events of
  // the AXTree when it is re-serialized.
  if (!web_contents) {
    return;
  }
  // Force a reset if web accessibility is already enabled to ensure that new
  // observers of accessibility events get the full accessibility tree from
  // scratch.
  const bool need_reset =
      web_contents->GetAccessibilityMode().has_mode(ui::AXMode::kWebContents);

  scoped_accessibility_mode_ =
      content::BrowserAccessibilityState::GetInstance()
          ->CreateScopedModeForWebContents(web_contents, accessibility_mode);

  if (need_reset) {
    web_contents->ResetAccessibility();
  }
}

MahiPDFObserver::~MahiPDFObserver() = default;

void MahiPDFObserver::AccessibilityEventReceived(
    const ui::AXUpdatesAndEvents& details) {
  if (details.ax_tree_id != tree_id_ || !callback_) {
    return;
  }

  for (const auto& update : details.updates) {
    updates_.push_back(update);
    if (update.ByteSize() >= kAXTreeUpdateByteSizeThreshold) {
      std::move(callback_).Run(updates_);
      return;
    }
  }
}

void MahiPDFObserver::OnTimerFired() {
  if (!callback_) {
    return;
  }
  std::move(callback_).Run(updates_);
}

MahiWebContentsManagerImpl::MahiWebContentsManagerImpl() = default;

MahiWebContentsManagerImpl::~MahiWebContentsManagerImpl() {
  focused_web_contents_ = nullptr;
}

void MahiWebContentsManagerImpl::OnFocusedPageLoadComplete(
    content::WebContents* web_contents) {
  auto* mahi_manager = chromeos::MahiManager::Get();
  if (!mahi_manager) {
    return;
  }

  // Do not notify mahi manager if the web_content is from a media app window,
  // to avoid overriding media app focus status.
  if (IsMediaAppWindow(web_contents->GetTopLevelNativeWindow())) {
    return;
  }

  if (ShouldSkip(web_contents)) {
    ClearFocusedWebContentState();
    return;
  }

  base::Time start_time = base::Time::Now();

  focused_web_contents_ = web_contents;
  focused_web_content_state_ =
      WebContentState(focused_web_contents_->GetLastCommittedURL(),
                      focused_web_contents_->GetTitle());
  focused_web_content_state_.favicon = GetFavicon(focused_web_contents_);
  focused_web_content_state_.is_incognito =
      IsFromIncognito(focused_web_contents_);

  crosapi::mojom::MahiPageInfoPtr page_info =
      ConvertWebContentStateToPageInfo(focused_web_content_state_);

  // Skip the distillable check for PDF content.
  if (IsPDFWebContents(web_contents)) {
    is_pdf_focused_web_contents_ = true;
    focused_web_content_state_.is_distillable.emplace(true);
    page_info->IsDistillable = true;
    mahi_manager->SetCurrentFocusedPageInfo(std::move(page_info));
    return;
  }

  is_pdf_focused_web_contents_ = false;
  // Notifies `MahiManager` the focused page has changed.
  mahi_manager->SetCurrentFocusedPageInfo(std::move(page_info));

  auto* rfh = web_contents->GetPrimaryMainFrame();
  if (!rfh || !rfh->IsRenderFrameLive()) {
    return;
  }

  content_extraction::GetInnerText(
      *rfh, /*node_id=*/std::nullopt,
      base::BindOnce(&MahiWebContentsManagerImpl::OnGetInnerText,
                     weak_pointer_factory_.GetWeakPtr(),
                     focused_web_content_state_.page_id, start_time));
}

void MahiWebContentsManagerImpl::ClearFocusedWebContentState() {
  focused_web_contents_ = nullptr;
  is_pdf_focused_web_contents_ = false;
  focused_web_content_state_ = WebContentState(/*url=*/GURL(), /*title=*/u"");
  if (!chromeos::MahiManager::Get()) {
    return;
  }

  // Notifies `MahiManager` the focused page has changed.
  chromeos::MahiManager::Get()->SetCurrentFocusedPageInfo(
      ConvertWebContentStateToPageInfo(focused_web_content_state_));
}

void MahiWebContentsManagerImpl::WebContentsDestroyed(
    content::WebContents* web_contents) {
  if (IsMediaAppWindow(web_contents->GetTopLevelNativeWindow())) {
    return;
  }
  if (focused_web_contents_ == web_contents) {
    ClearFocusedWebContentState();
  }
}

void MahiWebContentsManagerImpl::OnContextMenuClicked(
    int64_t display_id,
    ButtonType button_type,
    const std::u16string& question,
    const gfx::Rect& mahi_menu_bounds) {
  // Records the `button_type` has been clicked.
  base::UmaHistogramEnumeration(chromeos::mahi::kMahiContextMenuActivated,
                                button_type);

  if (!chromeos::MahiManager::Get()) {
    base::UmaHistogramEnumeration(
        chromeos::mahi::kMahiContextMenuActivatedFailed, button_type);
    return;
  }

  // Generates the context menu request.
  crosapi::mojom::MahiContextMenuRequestPtr context_menu_request =
      crosapi::mojom::MahiContextMenuRequest::New(
          /*display_id=*/display_id,
          /*action_type=*/
          chromeos::mahi::MatchButtonTypeToActionType(button_type),
          /*question=*/std::nullopt,
          /*mahi_menu_bounds=*/mahi_menu_bounds);
  if (button_type == chromeos::mahi::ButtonType::kQA) {
    context_menu_request->question = question;
  }

  // Forwards the UI request to `MahiManager`.
  chromeos::MahiManager::Get()->OnContextMenuClicked(
      std::move(context_menu_request));
}

bool MahiWebContentsManagerImpl::IsFocusedPageDistillable() {
  if (!focused_web_content_state_.is_distillable.has_value()) {
    return false;
  }
  return focused_web_content_state_.is_distillable.value();
}

void MahiWebContentsManagerImpl::OnGetInnerText(
    const base::UnguessableToken& page_id,
    const base::Time& start_time,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  if (focused_web_content_state_.page_id != page_id || !focused_web_contents_) {
    // TODO(b:336438243): Add UMA to track this.
    return;
  }
  if (!chromeos::MahiManager::Get()) {
    return;
  }

  base::UmaHistogramMicrosecondsTimes(
      chromeos::mahi::kMahiContentExtractionTriggeringLatency,
      base::Time::Now() - start_time);
  focused_web_content_state_.url = focused_web_contents_->GetLastCommittedURL();
  focused_web_content_state_.title = focused_web_contents_->GetTitle();
  focused_web_content_state_.favicon = GetFavicon(focused_web_contents_);
  bool distillable =
      result ? result->inner_text.length() > kCharCountThreshold : false;
  focused_web_content_state_.is_distillable.emplace(distillable);
  // Notifies `MahiManager` the focused page has changed.
  chromeos::MahiManager::Get()->SetCurrentFocusedPageInfo(
      ConvertWebContentStateToPageInfo(focused_web_content_state_));
}

void MahiWebContentsManagerImpl::OnGetSnapshot(
    const base::UnguessableToken& page_id,
    content::WebContents* web_contents,
    const base::Time& start_time,
    GetContentCallback callback,
    ui::AXTreeUpdate& snapshot) {
  if (focused_web_content_state_.page_id != page_id) {
    // TODO(b:336438243): Add UMA to track this.
    std::move(callback).Run(nullptr);
    return;
  }
  focused_web_content_state_.snapshot = snapshot;
  content_extraction_delegate_->ExtractContent(
      focused_web_content_state_,
      /*client_id, deprecated*/ base::UnguessableToken::Create(),
      std::move(callback));
}

void MahiWebContentsManagerImpl::RequestContent(
    const base::UnguessableToken& page_id,
    GetContentCallback callback) {
  if (focused_web_content_state_.page_id != page_id || !focused_web_contents_) {
    // TODO(b:336438243): Add UMA to track this.
    std::move(callback).Run(nullptr);
    return;
  }

  if (!content_extraction_delegate_) {
    content_extraction_delegate_ =
        std::make_unique<MahiContentExtractionDelegate>();
  }

  if (IsPDFWebContents(focused_web_contents_)) {
    RequestPDFContent(page_id, std::move(callback));
  } else {
    RequestWebContent(page_id, std::move(callback));
  }
}

void MahiWebContentsManagerImpl::RequestWebContent(
    const base::UnguessableToken& page_id,
    GetContentCallback callback) {
  base::Time start_time = base::Time::Now();
  focused_web_contents_->RequestAXTreeSnapshot(
      base::BindOnce(&MahiWebContentsManagerImpl::OnGetSnapshot,
                     weak_pointer_factory_.GetWeakPtr(),
                     focused_web_content_state_.page_id, focused_web_contents_,
                     start_time, std::move(callback)),
      ui::kAXModeWebContentsOnly,
      /* max_nodes= */ 5000, /* timeout= */ {},
      content::WebContents::AXTreeSnapshotPolicy::kAll);
}

void MahiWebContentsManagerImpl::RequestPDFContent(
    const base::UnguessableToken& page_id,
    GetContentCallback callback) {
  content::RenderFrameHost* rfh_pdf =
      GetPDFRenderFrameHost(focused_web_contents_);
  if (!rfh_pdf) {
    LOG(ERROR) << "Couldn't find RenderFrameHost contains PDF.";
    std::move(callback).Run(nullptr);
    return;
  }

  // If OOPIF PDF is enabled, we need to observe the focused web contents for
  // a11y changes. Otherwise, we need to observe the inner web contents.
  content::WebContents* web_contents_to_observe = focused_web_contents_;
  if (!base::FeatureList::IsEnabled(chrome_pdf::features::kPdfOopif)) {
    std::vector<content::WebContents*> inner_contents =
        focused_web_contents_ ? focused_web_contents_->GetInnerWebContents()
                              : std::vector<content::WebContents*>();

    if (inner_contents.size() != 1u) {
      LOG(ERROR) << "Couldn't find inner WebContents contains PDF.";
      std::move(callback).Run(nullptr);
      return;
    }

    web_contents_to_observe = inner_contents[0];
  }

  pdf_observer_ = std::make_unique<MahiPDFObserver>(
      web_contents_to_observe, ui::kAXModeWebContentsOnly,
      rfh_pdf->GetAXTreeID(),
      base::BindOnce(&MahiWebContentsManagerImpl::OnGetAXTreeUpdatesForPDF,
                     weak_pointer_factory_.GetWeakPtr(), std::move(callback)));
}

void MahiWebContentsManagerImpl::OnGetAXTreeUpdatesForPDF(
    GetContentCallback callback,
    const std::vector<ui::AXTreeUpdate>& updates) {
  content_extraction_delegate_->ExtractContent(
      focused_web_content_state_, std::move(updates),
      /*client_id, deprecated*/ base::UnguessableToken::Create(),
      std::move(callback));

  // No need to observes more a11y changes from PDF content.
  pdf_observer_.reset();
}

gfx::ImageSkia MahiWebContentsManagerImpl::GetFavicon(
    content::WebContents* web_contents) const {
  return favicon::TabFaviconFromWebContents(web_contents).AsImageSkia();
}

bool MahiWebContentsManagerImpl::ShouldSkip(
    content::WebContents* web_contents) {
  const auto url = web_contents->GetURL();

  static constexpr auto kSkipUrls = base::MakeFixedFlatSet<std::string_view>(
      {// blank and default pages.
       "about:blank", "chrome://newtab/",
       // Workspace
       "mail.google.com", "meet.google.com", "calendar.google.com",
       "tasks.google.com", "drive.google.com", "docs.google.com",
       "keep.google.com", "script.google.com", "voice.google.com"});
  // A tab should be skipped if it is empty, or have the domain in the
  // `kSkipUrls` list
  if (url.spec().empty()) {
    return true;
  }
  for (const auto& skip_url : kSkipUrls) {
    if (url.DomainIs(skip_url)) {
      return true;
    }
  }

  // Also skip urls that begins with `chrome` and `view-source`. They are
  // usually web UI and internal pages. E.g., `chrome://`, `chrome-internal://`
  // and `chrome-untrusted://`.
  return (url.spec().rfind("chrome", 0) == 0) ||
         (url.spec().rfind("view-source", 0) == 0);
}

}  // namespace mahi
