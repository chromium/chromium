// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"
#include "chrome/browser/context_hub/record_replay/tab_active_observer.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"

namespace context_hub {
namespace {

constexpr int32_t kMaxScreenshotWidth = 1280;
constexpr int32_t kMaxScreenshotHeight = 1280;

BrowserWindowInterface* GetLastActiveBrowser(Profile& profile) {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(&profile);
  return collection ? collection->GetLastActiveBrowser() : nullptr;
}

}  // namespace

SkillRecordingService::SkillRecordingService(Profile* profile)
    : profile_(CHECK_DEREF(profile)) {}

SkillRecordingService::~SkillRecordingService() {
  StopRecording();
}

void SkillRecordingService::StartRecording() {
  if (is_recording()) {
    return;
  }
  artifacts_.actions.clear();
  start_time_ = base::TimeTicks::Now();

  if (ProfileBrowserCollection* collection =
          ProfileBrowserCollection::GetForProfile(&profile_.get())) {
    browser_collection_observation_.Observe(collection);
  }

  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  browser_tab_strip_tracker_->Init();

  ObserveActiveTabInBrowser(GetLastActiveBrowser(*profile_));
}

void SkillRecordingService::StopRecording() {
  if (!is_recording()) {
    return;
  }
  start_time_.reset();
  browser_collection_observation_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  tab_observer_.reset();
  browser_tab_strip_tracker_.reset();
}

void SkillRecordingService::CaptureAction(
    browser::context_hub::mojom::SkillRecordingActionType type,
    std::string description) {
  if (!is_recording() ||
      artifacts_.actions.size() >= RecordingArtifacts::kMaxRecordedActions) {
    return;
  }
  const size_t action_index = artifacts_.actions.size();
  artifacts_.actions.emplace_back(type, std::move(description),
                                  base::TimeTicks::Now() - start_time_.value());

  if (!tab_observer_ || !tab_observer_->web_contents()) {
    return;
  }
  content::WebContents* web_contents = tab_observer_->web_contents();

  page_content_annotations::ScreenshotOptions::ScreenshotCollectionOptions
      collection_options;
  collection_options.max_width = kMaxScreenshotWidth;
  collection_options.max_height = kMaxScreenshotHeight;
  collection_options.screenshot_image_format =
      page_content_annotations::ScreenshotOptions::ScreenshotImageFormat::kJpeg;
  collection_options.screenshot_compression_quality = page_content_annotations::
      ScreenshotOptions::ScreenshotCompressionQuality::kMedium;

  page_content_annotations::FetchPageContextOptions options;
  options.screenshot_options =
      page_content_annotations::ScreenshotOptions::ViewportOnly(
          /*paint_preview_options=*/std::nullopt, collection_options);

  page_content_annotations::FetchPageContext(
      *web_contents, options, /*progress_listener=*/nullptr,
      BindScreenshotCapturedCallback(action_index));
}

page_content_annotations::FetchPageContextResultCallback
SkillRecordingService::BindScreenshotCapturedCallbackForTesting(
    size_t action_index) {
  return BindScreenshotCapturedCallback(action_index);
}

void SkillRecordingService::Shutdown() {
  StopRecording();
  artifacts_.actions.clear();
}

void SkillRecordingService::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!is_recording()) {
    return;
  }

  if (change.type() == TabStripModelChange::kReplaced && tab_observer_ &&
      change.GetReplace()->old_contents == tab_observer_->web_contents()) {
    ObserveWebContents(change.GetReplace()->new_contents);
    return;
  }

  if (change.type() == TabStripModelChange::kRemoved && tab_observer_ &&
      tab_observer_->web_contents()) {
    for (const TabStripModelChange::RemovedTab& contents :
         change.GetRemove()->contents) {
      if (contents.contents == tab_observer_->web_contents()) {
        tab_observer_.reset();
        break;
      }
    }
  }

  if (!selection.active_tab_changed()) {
    return;
  }

  BrowserWindowInterface* active_browser = GetLastActiveBrowser(*profile_);
  if (!active_browser ||
      active_browser->GetTabStripModel() != tab_strip_model) {
    return;
  }

  ObserveWebContents(selection.new_contents);
}

bool SkillRecordingService::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return browser && browser->GetProfile() == &profile_.get();
}

void SkillRecordingService::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  if (!is_recording()) {
    return;
  }
  ObserveActiveTabInBrowser(browser);
}

void SkillRecordingService::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (!is_recording()) {
    return;
  }
  BrowserWindowInterface* last_active = GetLastActiveBrowser(*profile_);
  ObserveActiveTabInBrowser(last_active != browser ? last_active : nullptr);
}

page_content_annotations::FetchPageContextResultCallback
SkillRecordingService::BindScreenshotCapturedCallback(size_t action_index) {
  return base::BindOnce(&SkillRecordingService::OnScreenshotCaptured,
                        weak_ptr_factory_.GetWeakPtr(), action_index);
}

void SkillRecordingService::OnScreenshotCaptured(
    size_t action_index,
    page_content_annotations::FetchPageContextResultCallbackArg result) {
  if (!is_recording() || action_index >= artifacts_.actions.size()) {
    return;
  }
  if (!result.has_value() || !result.value() ||
      !result.value()->screenshot_result.has_value()) {
    return;
  }
  const page_content_annotations::ScreenshotResult& screenshot =
      result.value()->screenshot_result.value();
  if (screenshot.screenshot_data.empty() || screenshot.mime_type.empty()) {
    return;
  }
  artifacts_.actions[action_index].screenshot_jpeg_bytes =
      std::string(base::as_string_view(screenshot.screenshot_data));
}

void SkillRecordingService::ObserveActiveTabInBrowser(
    BrowserWindowInterface* browser) {
  if (!browser || !browser->GetTabStripModel()) {
    tab_observer_.reset();
    return;
  }
  ObserveWebContents(browser->GetTabStripModel()->GetActiveWebContents());
}

void SkillRecordingService::ObserveWebContents(
    content::WebContents* web_contents) {
  if (tab_observer_ && tab_observer_->web_contents() == web_contents) {
    return;
  }
  tab_observer_.reset();
  if (web_contents) {
    tab_observer_ = std::make_unique<TabActiveObserver>(this, web_contents);
  }
}

}  // namespace context_hub
