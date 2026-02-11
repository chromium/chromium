// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_CLIENT_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_CLIENT_H_

#include <string_view>

class GURL;

namespace autofill {
class AutofillClient;
}

namespace content {
class WebContents;
}

namespace record_replay {

class RecordingDataManager;
class RecordReplayDriverFactory;
class RecordReplayManager;

// The root owner of most record & replay classes in the browser process.
//
// One instance per WebContents.
class RecordReplayClient {
 public:
  RecordReplayClient(const RecordReplayClient&) = delete;
  RecordReplayClient& operator=(const RecordReplayClient&) = delete;
  virtual ~RecordReplayClient();

  // Returns the manager owned by this client.
  virtual RecordReplayManager& GetManager() = 0;

  // Returns the driver factory owned by this client.
  virtual RecordReplayDriverFactory& GetDriverFactory() = 0;

  // Returns the data manager (if one exists), which may be shared with other
  // clients.
  virtual RecordingDataManager* GetRecordingDataManager() = 0;

  // Returns the primary main frame's last committed URL without credentials.
  virtual GURL GetPrimaryMainFrameUrl() = 0;

  // Returns the AutofillClient associated with the WebContents.
  virtual autofill::AutofillClient* GetAutofillClient() = 0;

  // Reports a message in a user-visible way.
  virtual void ReportToUser(std::string_view message) = 0;

 protected:
  RecordReplayClient();
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_CLIENT_H_
