// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_CHROME_RECORD_REPLAY_CLIENT_H_
#define CHROME_BROWSER_RECORD_REPLAY_CHROME_RECORD_REPLAY_CLIENT_H_

#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_driver_factory.h"
#include "chrome/browser/record_replay/record_replay_manager.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/common/record_replay/record_replay.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class GURL;

namespace autofill {
class AutofillClient;
}

namespace content {
class WebContents;
}

namespace record_replay {
class RecordingDataManager;
}

class ChromeRecordReplayClient : public record_replay::RecordReplayClient,
                                 public tabs::ContentsObservingTabFeature {
 public:
  explicit ChromeRecordReplayClient(tabs::TabInterface& tab);
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
  record_replay::RecordingDataManager* GetRecordingDataManager() override;
  GURL GetPrimaryMainFrameUrl() override;
  autofill::AutofillClient* GetAutofillClient() override;
  void ReportToUser(std::string_view message) override;

 private:
  void OnDiscardContents(tabs::TabInterface* tab,
                         content::WebContents* old_contents,
                         content::WebContents* new_contents) override;

  record_replay::RecordReplayDriverFactory driver_factory_{*this};
  record_replay::RecordReplayManager manager_{this};
};

#endif  // CHROME_BROWSER_RECORD_REPLAY_CHROME_RECORD_REPLAY_CLIENT_H_
