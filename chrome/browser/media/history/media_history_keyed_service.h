// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace media_history {

class MediaHistoryKeyedService : public KeyedService {
 public:
  explicit MediaHistoryKeyedService(content::BrowserContext* browser_context);
  ~MediaHistoryKeyedService() override;

  // Returns the instance attached to the given |profile|.
  static MediaHistoryKeyedService* Get(Profile* profile);

  MediaHistoryStore* GetMediaHistoryStore() {
    return media_history_store_.get();
  }

 private:
  std::unique_ptr<MediaHistoryStore> media_history_store_;

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryKeyedService);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_
