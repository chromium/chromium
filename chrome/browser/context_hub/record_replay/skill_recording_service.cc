// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/context_hub/record_replay/record_replay_mes_client.h"
#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"
#include "chrome/browser/context_hub/record_replay/skill_audio_capturer.h"
#include "chrome/browser/context_hub/record_replay/tab_active_observer.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_mode.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

BrowserWindowInterface* GetLastActiveBrowser(Profile& profile) {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(&profile);
  return collection ? collection->GetLastActiveBrowser() : nullptr;
}

GURL GetSanitizedRecordableUrl(content::WebContents* web_contents) {
  if (!web_contents) {
    return GURL();
  }
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  if (!main_rfh || main_rfh->IsErrorDocument()) {
    return GURL();
  }
  return SanitizeRecordedUrl(web_contents->GetLastCommittedURL());
}

std::optional<std::string> BuildTabSwitchDescription(
    content::WebContents* web_contents) {
  GURL sanitized_url = GetSanitizedRecordableUrl(web_contents);
  if (sanitized_url.is_empty()) {
    return std::nullopt;
  }

  std::string label;
  if (content::NavigationEntry* entry =
          web_contents->GetController().GetLastCommittedEntry();
      entry && !entry->GetTitle().empty()) {
    label = SanitizeRecordedText(base::UTF16ToUTF8(entry->GetTitle()));
  }
  if (label.empty()) {
    label = sanitized_url.spec();
  }
  return base::StrCat({"Switched to tab: ", label});
}

bool ShouldCaptureScreenshot(
    browser::context_hub::mojom::SkillRecordingActionType type) {
  switch (type) {
    case browser::context_hub::mojom::SkillRecordingActionType::kLeftClick:
    case browser::context_hub::mojom::SkillRecordingActionType::kTabChange:
      return true;
    case browser::context_hub::mojom::SkillRecordingActionType::kNavigation:
      return false;
  }
  NOTREACHED();
}

}  // namespace

void SkillRecordingService::Observer::OnTranscriptUpdate(
    const std::string& text,
    base::TimeDelta timestamp,
    bool is_final) {}

void SkillRecordingService::Observer::OnRecordingStarted() {}

void SkillRecordingService::Observer::OnActionUpdated(
    size_t action_index,
    const RecordedAction& action) {}

void SkillRecordingService::Observer::OnRecordingStopped(
    const RecordingArtifacts& artifacts) {}

void SkillRecordingService::Observer::OnSkillRecordingServiceShuttingDown() {}

SkillRecordingService::SkillRecordingService(
    Profile* profile,
    optimization_guide::RemoteModelExecutor* model_executor)
    : profile_(CHECK_DEREF(profile)),
      mes_client_(model_executor
                      ? std::make_unique<RecordReplayMesClient>(model_executor)
                      : nullptr) {}

SkillRecordingService::~SkillRecordingService() {
  Shutdown();
}

void SkillRecordingService::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void SkillRecordingService::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void SkillRecordingService::StartRecording() {
  if (is_shut_down_ || is_recording()) {
    return;
  }
  artifacts_ = RecordingArtifacts();
  start_time_ = base::TimeTicks::Now();
  scoped_accessibility_mode_ =
      content::BrowserAccessibilityState::GetInstance()
          ->CreateScopedModeForBrowserContext(
              &profile_.get(),
              ui::kAXModeBasic | ui::AXMode::kExtendedProperties);

  if (ProfileBrowserCollection* collection =
          ProfileBrowserCollection::GetForProfile(&profile_.get())) {
    browser_collection_observation_.Observe(collection);
  }

  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  browser_tab_strip_tracker_->Init();

  ObserveActiveTabInBrowser(GetLastActiveBrowser(*profile_));

  if (!audio_capturer_) {
    // Safe because this service owns `audio_capturer_` and resets it in
    // `Shutdown()`.
    audio_capturer_ = std::make_unique<SkillAudioCapturer>(
        &profile_.get(),
        base::BindRepeating(&SkillRecordingService::OnTranscriptUpdate,
                            base::Unretained(this)));
  }
  audio_capturer_->Start();

  observers_.Notify(&Observer::OnRecordingStarted);

  if (mes_client_) {
    mes_client_->CancelPendingRequest();
  }
}

void SkillRecordingService::StopRecording() {
  if (!is_recording()) {
    return;
  }
  if (audio_capturer_) {
    audio_capturer_->Stop();
  }
  start_time_.reset();
  browser_collection_observation_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  tab_observer_.reset();
  browser_tab_strip_tracker_.reset();
  scoped_accessibility_mode_.reset();

  for (RecordedAction& action : artifacts_.actions) {
    action.is_loading_screenshot = false;
  }

  observers_.Notify(&Observer::OnRecordingStopped, artifacts_);
}

void SkillRecordingService::CaptureAction(
    browser::context_hub::mojom::SkillRecordingActionType type,
    std::string description) {
  if (!is_recording() ||
      artifacts_.actions.size() >= RecordingArtifacts::kMaxRecordedActions) {
    return;
  }
  const size_t action_index = artifacts_.actions.size();
  RecordedAction& action = artifacts_.actions.emplace_back(
      type, std::move(description),
      base::TimeTicks::Now() - start_time_.value());

  content::WebContents* web_contents =
      tab_observer_ ? tab_observer_->web_contents() : nullptr;
  action.current_url = GetSanitizedRecordableUrl(web_contents);
  const bool should_capture_screenshot = web_contents &&
                                         ShouldCaptureScreenshot(type) &&
                                         !action.current_url.is_empty();
  action.is_loading_screenshot = should_capture_screenshot;
  base::WeakPtr<content::WebContents> weak_web_contents =
      should_capture_screenshot ? web_contents->GetWeakPtr() : nullptr;

  observers_.Notify(&Observer::OnActionUpdated, action_index, action);

  if (!should_capture_screenshot || !is_recording() ||
      action_index >= artifacts_.actions.size() ||
      !artifacts_.actions[action_index].is_loading_screenshot) {
    return;
  }
  if (!weak_web_contents || weak_web_contents->IsBeingDestroyed() ||
      !weak_web_contents->GetPrimaryMainFrame()) {
    RecordedAction& updated_action = artifacts_.actions[action_index];
    updated_action.is_loading_screenshot = false;
    observers_.Notify(&Observer::OnActionUpdated, action_index, updated_action);
    return;
  }

  constexpr int32_t kMaxScreenshotWidth = 1280;
  constexpr int32_t kMaxScreenshotHeight = 1280;
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
      *weak_web_contents, options, /*progress_listener=*/nullptr,
      BindScreenshotCapturedCallback(action_index));
}

void SkillRecordingService::GenerateSkillFromRecording(
    GenerateSkillCallback callback) {
  if (is_shut_down_ || is_recording() || !mes_client_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  mes_client_->GenerateSkillFromRecording(artifacts_, std::move(callback));
}

void SkillRecordingService::OnTranscriptUpdateForTesting(
    const std::string& text,
    base::TimeDelta timestamp,
    bool is_final) {
  OnTranscriptUpdate(text, timestamp, is_final);
}

page_content_annotations::FetchPageContextResultCallback
SkillRecordingService::BindScreenshotCapturedCallbackForTesting(
    size_t action_index) {
  return BindScreenshotCapturedCallback(action_index);
}

void SkillRecordingService::Shutdown() {
  if (is_shut_down_) {
    return;
  }
  is_shut_down_ = true;
  StopRecording();
  artifacts_ = RecordingArtifacts();
  audio_capturer_.reset();
  mes_client_.reset();
  observers_.Notify(&Observer::OnSkillRecordingServiceShuttingDown);
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

  if (tab_strip_model->closing_all()) {
    tab_observer_.reset();
    return;
  }

  ObserveWebContents(selection.new_contents);
  if (selection.old_tab != selection.new_tab) {
    MaybeCaptureTabSwitch();
  }
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
  SwitchActiveBrowser(browser);
}

void SkillRecordingService::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (!is_recording()) {
    return;
  }
  BrowserWindowInterface* last_active = GetLastActiveBrowser(*profile_);
  if (!last_active || last_active == browser) {
    tab_observer_.reset();
    return;
  }
  SwitchActiveBrowser(last_active);
}

void SkillRecordingService::OnTranscriptUpdate(const std::string& text,
                                               base::TimeDelta timestamp,
                                               bool is_final) {
  if (!is_recording()) {
    return;
  }
  if (is_final && artifacts_.transcripts.size() <
                      RecordingArtifacts::kMaxRecordedTranscripts) {
    std::string_view trimmed_text =
        base::TrimWhitespaceASCII(text, base::TRIM_ALL);
    if (!trimmed_text.empty()) {
      artifacts_.transcripts.emplace_back(std::string(trimmed_text), timestamp);
    }
  }
  observers_.Notify(&Observer::OnTranscriptUpdate, text, timestamp, is_final);
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
  RecordedAction& action = artifacts_.actions[action_index];
  action.is_loading_screenshot = false;
  if (result.has_value() && result.value() &&
      result.value()->screenshot_result.has_value()) {
    const page_content_annotations::ScreenshotResult& screenshot =
        result.value()->screenshot_result.value();
    if (!screenshot.screenshot_data.empty() && !screenshot.mime_type.empty()) {
      action.screenshot_jpeg_bytes =
          std::string(base::as_string_view(screenshot.screenshot_data));
    }
  }
  observers_.Notify(&Observer::OnActionUpdated, action_index, action);
}

void SkillRecordingService::MaybeCaptureTabSwitch() {
  content::WebContents* web_contents =
      tab_observer_ ? tab_observer_->web_contents() : nullptr;
  if (std::optional<std::string> description =
          BuildTabSwitchDescription(web_contents)) {
    CaptureAction(
        browser::context_hub::mojom::SkillRecordingActionType::kTabChange,
        std::move(description).value());
  }
}

void SkillRecordingService::SwitchActiveBrowser(
    BrowserWindowInterface* browser) {
  content::WebContents* previous_contents =
      tab_observer_ ? tab_observer_->web_contents() : nullptr;
  ObserveActiveTabInBrowser(browser);
  content::WebContents* new_contents =
      tab_observer_ ? tab_observer_->web_contents() : nullptr;
  if (new_contents && new_contents != previous_contents) {
    MaybeCaptureTabSwitch();
  }
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
