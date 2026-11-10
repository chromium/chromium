// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_RECORDING_SERVICE_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_RECORDING_SERVICE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"

class BrowserTabStripTracker;
class BrowserWindowInterface;
class Profile;
class ProfileBrowserCollection;

namespace content {
class WebContents;
}  // namespace content

namespace context_hub {

class TabActiveObserver;

// Per-profile service that manages in-memory recording sessions for capturing
// user actions to generate skills.
class SkillRecordingService : public KeyedService,
                              public TabStripModelObserver,
                              public BrowserTabStripTrackerDelegate,
                              public BrowserCollectionObserver {
 public:
  // `profile` must be non-null and must outlive this `SkillRecordingService`.
  explicit SkillRecordingService(Profile* profile);
  SkillRecordingService(const SkillRecordingService&) = delete;
  SkillRecordingService& operator=(const SkillRecordingService&) = delete;
  ~SkillRecordingService() override;

  // Starts a new recording session if one is not already active, clearing any
  // artifacts from a previous session.
  void StartRecording();

  // Stops the current recording session if one is active. Captured artifacts
  // remain accessible via `artifacts()` until the next recording session
  // starts or the service shuts down.
  void StopRecording();

  const RecordingArtifacts& artifacts() const { return artifacts_; }

  bool is_recording() const { return start_time_.has_value(); }

  std::optional<base::TimeTicks> start_time() const { return start_time_; }

  // Captures a user action of the given `type` and requests a viewport
  // screenshot.
  void CaptureAction(browser::context_hub::mojom::SkillRecordingActionType type,
                     std::string description);

  // Returns a screenshot completion callback bound to `weak_ptr_factory_` for
  // testing screenshot handling and callback invalidation across sessions.
  page_content_annotations::FetchPageContextResultCallback
  BindScreenshotCapturedCallbackForTesting(size_t action_index);

  // KeyedService:
  void Shutdown() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  page_content_annotations::FetchPageContextResultCallback
  BindScreenshotCapturedCallback(size_t action_index);

  // Handles the screenshot capture result for the action at `action_index`.
  void OnScreenshotCaptured(
      size_t action_index,
      page_content_annotations::FetchPageContextResultCallbackArg result);
  void ObserveActiveTabInBrowser(BrowserWindowInterface* browser);
  void ObserveWebContents(content::WebContents* web_contents);

  const raw_ref<Profile> profile_;
  std::optional<base::TimeTicks> start_time_;
  RecordingArtifacts artifacts_;

  std::unique_ptr<TabActiveObserver> tab_observer_;
  std::unique_ptr<BrowserTabStripTracker> browser_tab_strip_tracker_;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::WeakPtrFactory<SkillRecordingService> weak_ptr_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_RECORDING_SERVICE_H_
