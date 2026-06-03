// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_CHROME_RECORD_REPLAY_CLIENT_H_
#define CHROME_BROWSER_RECORD_REPLAY_CHROME_RECORD_REPLAY_CLIENT_H_

#include <optional>

#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/record_replay/content/browser/content_record_replay_driver_factory.h"
#include "components/record_replay/core/browser/record_replay_client.h"
#include "components/record_replay/core/browser/record_replay_manager.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/record_replay/core/browser/task_discovery_service.h"
#include "components/record_replay/core/browser/task_service.h"
#include "components/record_replay/core/common/record_replay.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class GURL;

namespace autofill {
class AutofillClient;
}

namespace content {
class NavigationHandle;
class WebContents;
}

namespace record_replay {
class TaskStore;
}

// The Chrome-specific concrete implementation of `RecordReplayClient`.
//
// Initialized as a `tabs::TabFeature` (1 per tab) and runs on the UI thread.
class ChromeRecordReplayClient : public record_replay::RecordReplayClient,
                                 public tabs::ContentsObservingTabFeature {
 public:
  explicit ChromeRecordReplayClient(tabs::TabInterface& tab);
  ChromeRecordReplayClient(
      tabs::TabInterface& tab,
      std::unique_ptr<record_replay::TaskDiscoveryService> service);
  ChromeRecordReplayClient(const ChromeRecordReplayClient&) = delete;
  ChromeRecordReplayClient& operator=(const ChromeRecordReplayClient&) = delete;
  ~ChromeRecordReplayClient() override;

  DECLARE_USER_DATA(ChromeRecordReplayClient);

  static void BindRecordReplayDriver(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingAssociatedReceiver<record_replay::mojom::RecordReplayDriver>
          pending_receiver);

  // record_replay::RecordReplayClient:
  record_replay::RecordReplayManager& GetManager() override;
  record_replay::RecordReplayDriverFactory& GetDriverFactory() override;
  record_replay::TaskStore* GetTaskStore() override;
  GURL GetPrimaryMainFrameUrl() override;
  autofill::AutofillClient* GetAutofillClient() override;
  void ReportToUser(std::string_view message) override;
  base::WeakPtr<record_replay::RecordReplayClient> GetWeakPtr() override;
  void OfferExecuting(
      const record_replay::TaskDefinition& definition,
      const record_replay::TaskParameterValues& values) override;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

  // Called when the execution offer is accepted via the toast button.
  void OnExecutionAccepted();

 private:
  void OnShouldOfferTask(bool offered);
  void PerformParametersExtraction(const GURL& url);

  void OnDiscardContents(tabs::TabInterface* tab,
                         content::WebContents* old_contents,
                         content::WebContents* new_contents) override;

  record_replay::ContentRecordReplayDriverFactory driver_factory_{*this};
  record_replay::RecordReplayManager manager_{this};
  std::unique_ptr<record_replay::TaskDiscoveryService> task_discovery_service_;

  // Offered task metadata stored while toast is shown to the user.
  std::optional<record_replay::TaskDefinition> offered_task_definition_;
  std::optional<record_replay::TaskParameterValues>
      offered_task_parameter_values_;

  base::WeakPtrFactory<ChromeRecordReplayClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_RECORD_REPLAY_CHROME_RECORD_REPLAY_CLIENT_H_
