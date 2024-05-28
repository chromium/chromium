// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_HISTORY_DELETION_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_HISTORY_HISTORY_DELETION_BRIDGE_H_

#include "base/scoped_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"

class Profile;

namespace history {
class HistoryService;
}  // namespace history

// Native counterpart of HistoryDeletionBridge.java. Receives history deletion
// events that originate in native code and forwards them to Java.
class HistoryDeletionBridge : public history::HistoryServiceObserver {
 public:
  HistoryDeletionBridge(const base::android::JavaRef<jobject>& j_this,
                        Profile* profile);

  HistoryDeletionBridge(const HistoryDeletionBridge&) = delete;
  HistoryDeletionBridge& operator=(const HistoryDeletionBridge&) = delete;

  void Destroy(JNIEnv* env);

  // history::HistoryServiceObserver.
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Sanitize the DeletionInfo of empty/invalid urls before passing to java.
  // Fix for empty java strings being passed to the content capture service
  // (crbug.com/1136486).
  static history::DeletionInfo SanitizeDeletionInfo(
      const history::DeletionInfo& deletion_info);

 private:
  ~HistoryDeletionBridge() override;

  // Reference to the Java half of this bridge. Always valid.
  base::android::ScopedJavaGlobalRef<jobject> jobj_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      scoped_history_service_observer_{this};
};

#endif  // CHROME_BROWSER_ANDROID_HISTORY_HISTORY_DELETION_BRIDGE_H_
