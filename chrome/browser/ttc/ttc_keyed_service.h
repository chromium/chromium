// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_TTC_KEYED_SERVICE_H_
#define CHROME_BROWSER_TTC_TTC_KEYED_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace ttc {

class TtcKeyedService : public KeyedService {
 public:
  static TtcKeyedService* Get(content::BrowserContext* context);

  explicit TtcKeyedService(Profile* profile);
  TtcKeyedService(const TtcKeyedService&) = delete;
  TtcKeyedService& operator=(const TtcKeyedService&) = delete;
  ~TtcKeyedService() override;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_TTC_KEYED_SERVICE_H_
