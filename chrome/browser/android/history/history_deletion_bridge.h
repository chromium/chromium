// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_HISTORY_DELETION_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_HISTORY_HISTORY_DELETION_BRIDGE_H_

#include "base/macros.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"

namespace history {
class HistoryService;
}  // namespace history

class Profile;

// Native counterpart of HistoryDeletionBridge.java. Receives history deletion
// events that originate in native code and forwards them to Java.
class HistoryDeletionBridge : public history::HistoryServiceObserver {
 public:
  explicit HistoryDeletionBridge(const base::android::JavaRef<jobject>& j_this);

  // history::HistoryServiceObserver.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

 private:
  ~HistoryDeletionBridge() override;

  // Reference to the Java half of this bridge. Always valid.
  base::android::ScopedJavaGlobalRef<jobject> jobj_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDeletionBridge);
};

#endif  // CHROME_BROWSER_ANDROID_HISTORY_HISTORY_DELETION_BRIDGE_H_
