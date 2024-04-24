// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_client_impl.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chrome/browser/chromeos/mahi/mahi_content_extraction_delegate.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

#if DCHECK_IS_ON()
#include "base/functional/callback_helpers.h"
#include "chromeos/constants/chromeos_features.h"
#endif

namespace mahi {

namespace {
MahiWebContentsManager* g_mahi_web_content_manager_for_testing = nullptr;
}

// static
MahiWebContentsManager* MahiWebContentsManager::Get() {
  if (g_mahi_web_content_manager_for_testing) {
    return g_mahi_web_content_manager_for_testing;
  }
  static base::NoDestructor<MahiWebContentsManager> instance;
  return instance.get();
}

MahiWebContentsManager::MahiWebContentsManager() = default;

MahiWebContentsManager::~MahiWebContentsManager() = default;

void MahiWebContentsManager::Initialize() {
  client_ = std::make_unique<
      MahiBrowserClientImpl>(/*request_content_callback=*/
                             base::BindRepeating(
                                 &MahiWebContentsManager::RequestContent,
                                 weak_pointer_factory_.GetWeakPtr()));
  content_extraction_delegate_ = std::make_unique<
      MahiContentExtractionDelegate>(/*distillable_check_callback=*/
                                     base::BindRepeating(
                                         &MahiWebContentsManager::
                                             OnFinishDistillableCheck,
                                         weak_pointer_factory_.GetWeakPtr()));
  is_initialized_ = true;
}

void MahiWebContentsManager::OnFocusedPageLoadComplete(
    content::WebContents* web_contents) {
  if (!is_initialized_ ||
      (!g_mahi_web_content_manager_for_testing &&
       !chromeos::MahiManager::Get()->IsSupportedWithCorrectFeatureKey())) {
    return;
  }
  base::Time start_time = base::Time::Now();

  // Page info may not be properly updated yet if the user forwards/backwards
  // the tab through cache. Thus, if focused page's URL does not change, we
  // don't create a new `focused_web_content_state_` here, and instead rely on
  // the callback of `RequestAXTreeSnapshot` to update if needed.
  if (web_contents->GetLastCommittedURL() != focused_web_content_state_.url) {
    // Creates a new focused web content state, and fires
    // `OnFocusedPageChanged()`
    // event immediately so that `MahiManager` knows the focused page has
    // changed.
    focused_web_content_state_ = WebContentState(
        web_contents->GetLastCommittedURL(), web_contents->GetTitle());
    focused_web_content_state_.favicon = GetFavicon(web_contents);

    // If the page is in the skip list, sets its distillability to false and
    // notifies `MahiManager` immediately without requesting the snapshot.
    if (ShouldSkip(web_contents)) {
      focused_web_content_state_.is_distillable.emplace(false);
      client_->OnFocusedPageChanged(focused_web_content_state_);
      return;
    }

    // Notifies `MahiManger` the focused page has changed.
    client_->OnFocusedPageChanged(focused_web_content_state_);
  }

  // Requests the a11y tree snapshot.
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  if (render_frame_host) {
    focused_web_content_state_.ukm_source_id =
        render_frame_host->GetPageUkmSourceId();
  }
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&MahiWebContentsManager::OnGetSnapshot,
                     weak_pointer_factory_.GetWeakPtr(),
                     focused_web_content_state_.page_id, web_contents,
                     start_time),
      ui::kAXModeWebContentsOnly,
      /* max_nodes= */ 5000, /* timeout= */ {});
}

void MahiWebContentsManager::ClearFocusedWebContentState() {
  focused_web_content_state_ = WebContentState(/*url=*/GURL(), /*title=*/u"");
  if (!is_initialized_) {
    return;
  }

  // Notifies `MahiManger` the focused page has changed.
  client_->OnFocusedPageChanged(focused_web_content_state_);
}

void MahiWebContentsManager::OnContextMenuClicked(
    int64_t display_id,
    ButtonType button_type,
    const std::u16string& question) {
  // Updates requested web content state. Don't update if the button type is
  // `kSettings`.
  if (button_type != ButtonType::kSettings) {
    FocusedPageGotRequest();
  }
  // Forwards the UI request to `MahiBrowserDelegate`.
  client_->OnContextMenuClicked(display_id, button_type, question);

  // Records the `button_type` has been clicked.
  base::UmaHistogramEnumeration(kMahiContextMenuActivated, button_type);
}

bool MahiWebContentsManager::IsFocusedPageDistillable() {
  if (!focused_web_content_state_.is_distillable.has_value()) {
    return false;
  }
  return focused_web_content_state_.is_distillable.value();
}

bool MahiWebContentsManager::GetPrefValue() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  return profile->GetPrefs()->GetBoolean(ash::prefs::kMahiEnabled);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return mahi_pref_lacros_;
#endif
}

// static
void MahiWebContentsManager::SetInstanceForTesting(
    MahiWebContentsManager* test_manager) {
  g_mahi_web_content_manager_for_testing = test_manager;
}

// static
void MahiWebContentsManager::ResetInstanceForTesting() {
  g_mahi_web_content_manager_for_testing = nullptr;
}

void MahiWebContentsManager::OnGetSnapshot(
    const base::UnguessableToken& page_id,
    content::WebContents* web_contents,
    const base::Time& start_time,
    const ui::AXTreeUpdate& snapshot) {
  // Updates states and checks the distillability of the snapshot.
  if (page_id == focused_web_content_state_.page_id) {
    // If the acquired url does not match the one within the snapshot, updates
    // `focused_web_content_state_` to ensure they match.
    if (focused_web_content_state_.url != GURL(snapshot.tree_data.url)) {
      focused_web_content_state_ =
          WebContentState(GURL(snapshot.tree_data.url),
                          base::UTF8ToUTF16(snapshot.tree_data.title));
      // Attempts to update the favicon if the url of the focused web contents
      // have updated.
      if (web_contents && web_contents->GetLastCommittedURL() ==
                              focused_web_content_state_.url) {
        focused_web_content_state_.favicon = GetFavicon(web_contents);
      }
    }
    focused_web_content_state_.snapshot = snapshot;

    // When debugging is enabled, directly extracts contents.
#if DCHECK_IS_ON()
    if (chromeos::features::IsMahiDebuggingEnabled()) {
      content_extraction_delegate_->ExtractContent(
          focused_web_content_state_, client_->client_id(), base::DoNothing());
    }
#endif
    content_extraction_delegate_->CheckDistillablity(focused_web_content_state_,
                                                     start_time);
  } else if (page_id == requested_web_content_state_.page_id) {
    requested_web_content_state_.snapshot = snapshot;
    content_extraction_delegate_->CheckDistillablity(
        requested_web_content_state_, start_time);
  }
}

void MahiWebContentsManager::OnFinishDistillableCheck(
    const base::UnguessableToken& page_id,
    bool is_distillable) {
  // Updates states and notifies the page state update.
  if (page_id == focused_web_content_state_.page_id) {
    focused_web_content_state_.is_distillable.emplace(is_distillable);
    client_->OnFocusedPageChanged(focused_web_content_state_);
  } else if (page_id == requested_web_content_state_.page_id) {
    requested_web_content_state_.is_distillable.emplace(is_distillable);
    client_->OnFocusedPageChanged(requested_web_content_state_);
  }
}

void MahiWebContentsManager::RequestContent(
    const base::UnguessableToken& page_id,
    GetContentCallback callback) {
  if (page_id == focused_web_content_state_.page_id) {
    // Updates requested web content state, if the focused state is requested.
    FocusedPageGotRequest();
    // As the requested web content state has been updated, sends the requested
    // web content state instead.
    content_extraction_delegate_->ExtractContent(requested_web_content_state_,
                                                 client_->client_id(),
                                                 std::move(callback));
  } else if (page_id == requested_web_content_state_.page_id) {
    content_extraction_delegate_->ExtractContent(requested_web_content_state_,
                                                 client_->client_id(),
                                                 std::move(callback));
  } else {
    // Early return if no matching `page_id` is found.
    std::move(callback).Run(nullptr);
    return;
  }
}

void MahiWebContentsManager::FocusedPageGotRequest() {
  requested_web_content_state_ = focused_web_content_state_;
}

gfx::ImageSkia MahiWebContentsManager::GetFavicon(
    content::WebContents* web_contents) const {
  return favicon::TabFaviconFromWebContents(web_contents).AsImageSkia();
}

bool MahiWebContentsManager::ShouldSkip(content::WebContents* web_contents) {
  const std::string& url = web_contents->GetURL().spec();

  static constexpr auto kSkipUrls = base::MakeFixedFlatSet<std::string_view>({
      // blank and default pages.
      "about:blank",
      "chrome://newtab/",
  });
  // A tab should be skipped if it is empty, blank or default page.
  if (url.empty() || base::Contains(kSkipUrls, url)) {
    return true;
  }

  // Also skip urls that begins with `chrome`. They are usually web UI and
  // internal pages. E.g., `chrome://`, `chrome-internal://` and
  // `chrome-untrusted://`.
  return url.rfind("chrome", 0) == 0;
}

}  // namespace mahi
