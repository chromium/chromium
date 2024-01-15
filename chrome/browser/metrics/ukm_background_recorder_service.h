// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_UKM_BACKGROUND_RECORDER_SERVICE_H_
#define CHROME_BROWSER_METRICS_UKM_BACKGROUND_RECORDER_SERVICE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class Profile;
class UkmBackgroundRecorderBrowserTest;

namespace content {
class BrowserContext;
}  // namespace content

namespace history {
class HistoryService;
struct VisibleVisitCountToHostResult;
}  // namespace history

namespace url {
class Origin;
}  // namespace url

namespace ukm {

// Service for background features that want to record UKM events. This service
// relies on the HistoryService, so that it can check whether the URL of the
// event is present in the Browser's history, and accordingly decide whether to
// generate a new SourceId or not.
class UkmBackgroundRecorderService : public KeyedService {
 public:
  using GetBackgroundSourceIdCallback =
      base::OnceCallback<void(std::optional<ukm::SourceId>)>;

  // |profile| is needed to access the appropriate services |this| depends on.
  explicit UkmBackgroundRecorderService(Profile* profile);

  UkmBackgroundRecorderService(const UkmBackgroundRecorderService&) = delete;
  UkmBackgroundRecorderService& operator=(const UkmBackgroundRecorderService&) =
      delete;

  ~UkmBackgroundRecorderService() override;

  void Shutdown() override;

  // Checks whether events related to |origin| can be recorded with UKM, by
  // checking if |origin| is present in the profile's history. This
  // should be used with background features if there will not be an open
  // window at the time of the recording. |callback| will be run with a valid
  // source ID if recording is allowed, or a nullopt otherwise.
  void GetBackgroundSourceIdIfAllowed(const url::Origin& origin,
                                      GetBackgroundSourceIdCallback callback);

 private:
  friend UkmBackgroundRecorderBrowserTest;

  // Callback for querying the history service. |did_determine| is whether the
  // history service was able to complete the query, and |num_visits| is the
  // number of visits for the provided |origin|.
  void DidGetVisibleVisitCount(
      const url::Origin& origin,
      UkmBackgroundRecorderService::GetBackgroundSourceIdCallback callback,
      history::VisibleVisitCountToHostResult result);

  raw_ptr<history::HistoryService> history_service_;

  // Task tracker used for querying URLs in the history service.
  base::CancelableTaskTracker task_tracker_;

  base::WeakPtrFactory<UkmBackgroundRecorderService> weak_ptr_factory_{this};
};

class UkmBackgroundRecorderFactory : public ProfileKeyedServiceFactory {
 public:
  static UkmBackgroundRecorderFactory* GetInstance();
  static UkmBackgroundRecorderService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<UkmBackgroundRecorderFactory>;

  UkmBackgroundRecorderFactory();
  ~UkmBackgroundRecorderFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ukm

#endif  // CHROME_BROWSER_METRICS_UKM_BACKGROUND_RECORDER_SERVICE_H_
