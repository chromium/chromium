// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACTORY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACTORY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache.h"
#include "components/performance_manager/public/graph/graph.h"
#include "content/public/browser/browser_context.h"

namespace content {
class BrowserContext;
}

namespace performance_manager {

class SiteDataCacheInspector;

// This class is responsible for tracking the SiteDataCache instances associated
// with each browser context. It is meant to be used as a bridge between the
// browser contexts living on the UI thread and the PerformanceManager
// sequence.
//
// This can be created on any sequence but it then should be passed to the
// graph and used on the PerformanceManager sequence.
class SiteDataCacheFactory : public GraphOwnedDefaultImpl {
 public:
  SiteDataCacheFactory();
  ~SiteDataCacheFactory() override;

  // Retrieves the currently registered instance.
  // The caller needs to ensure that the lifetime of the registered instance
  // exceeds the use of this function and the retrieved pointer.
  // This function can be called from any sequence with those caveats.
  static SiteDataCacheFactory* GetInstance();

  // Functions that should be called when a new browser context is created or
  // destroyed. They should be called from the UI thread, a task will then be
  // posted to the task_runner owned by |factory| to create the data store
  // associated with this browser context.
  //
  // If this browser context is inheriting from a parent context (e.g. if it's
  // off the record) then this parent context should be specified via
  // |parent_context|.
  static void OnBrowserContextCreatedOnUIThread(
      SiteDataCacheFactory* factory,
      content::BrowserContext* browser_context,
      content::BrowserContext* parent_context);
  static void OnBrowserContextDestroyedOnUIThread(
      SiteDataCacheFactory* factory,
      content::BrowserContext* browser_context);

  // Returns a pointer to the data cache associated with |browser_context_id|,
  // or null if there's no cache for this context yet.
  //
  // Should only be called from the Performance Manager sequence.
  SiteDataCache* GetDataCacheForBrowserContext(
      const std::string& browser_context_id) const;

  // Returns the data cache inspector associated with |browser_context_id|, or
  // null if there's no data cache inspector for this context yet.
  //
  // Should only be called from the Performance Manager sequence.
  SiteDataCacheInspector* GetInspectorForBrowserContext(
      const std::string& browser_context_id) const;

  // Sets the inspector instance associated with a given browser context.
  // If |inspector| is nullptr the association is cleared.
  // The caller must ensure that |inspector|'s registration is cleared before
  // |inspector| or |browser_context| are deleted.
  // The intent is for this to be called from the SiteDataCache implementation
  // class' constructors and destructors.
  //
  // Should only be called from the Performance Manager sequence.
  void SetDataCacheInspectorForBrowserContext(
      SiteDataCacheInspector* inspector,
      const std::string& browser_context_id);

  // Testing functions to check if the data cache associated with
  // |browser_context_id| is recording. This will be completed asynchronously
  // and |cb| will be called on the caller's sequence. This should be called
  // only on the UI thread.
  void IsDataCacheRecordingForTesting(const std::string& browser_context_id,
                                      base::OnceCallback<void(bool)> cb);

 private:
  // Implementation of the corresponding *OnUIThread public static functions
  // that runs on this object's task runner.
  void OnBrowserContextCreated(const std::string& browser_context_id,
                               const base::FilePath& context_path,
                               base::Optional<std::string> parent_context_id);
  void OnBrowserContextDestroyed(const std::string& browser_context_id);

  // A map that associates a BrowserContext's ID with a SiteDataCache. This
  // object owns the caches.
  base::flat_map<std::string, std::unique_ptr<SiteDataCache>> data_cache_map_;

  // A map that associates a BrowserContext's ID with a SiteDataCacheInspector.
  base::flat_map<std::string, SiteDataCacheInspector*>
      data_cache_inspector_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SiteDataCacheFactory);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACTORY_H_
