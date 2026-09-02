// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_TTC_KEYED_SERVICE_H_
#define CHROME_BROWSER_TTC_TTC_KEYED_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace ttc {

class SessionController;

class TtcKeyedService : public KeyedService {
 public:
  static TtcKeyedService* Get(content::BrowserContext* context);

  explicit TtcKeyedService(Profile* profile);
  TtcKeyedService(const TtcKeyedService&) = delete;
  TtcKeyedService& operator=(const TtcKeyedService&) = delete;
  ~TtcKeyedService() override;

  // KeyedService:
  void Shutdown() override;

  void StartSession();

  // Ends the active session. After this call, session_controller() is nullptr.
  // This is a no-op if no session is currently in progress.
  void EndSession();

  SessionController* session_controller() { return session_controller_.get(); }

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<SessionController> session_controller_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_TTC_KEYED_SERVICE_H_
