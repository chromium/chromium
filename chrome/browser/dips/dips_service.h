// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_SERVICE_H_
#define CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/dips/dips_storage.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace content_settings {
class CookieSettings;
}

namespace signin {
class PersistentRepeatingTimer;
}

class DIPSService : public KeyedService {
 public:
  ~DIPSService() override;

  static DIPSService* Get(content::BrowserContext* context);

  base::SequenceBound<DIPSStorage>* storage() { return &storage_; }

  bool ShouldBlockThirdPartyCookies() const;

  void RemoveEvents(const base::Time& delete_begin,
                    const base::Time& delete_end,
                    const UrlPredicate& predicate,
                    const DIPSEventRemovalType type);

 private:
  // So DIPSServiceFactory::BuildServiceInstanceFor can call the constructor.
  friend class DIPSServiceFactory;
  explicit DIPSService(content::BrowserContext* context);
  std::unique_ptr<signin::PersistentRepeatingTimer> CreateTimer(
      Profile* profile);
  void Shutdown() override;

  scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner();
  void InitializeStorageWithEngagedSites();
  void InitializeStorage(base::Time time, std::vector<std::string> sites);

  raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  std::unique_ptr<signin::PersistentRepeatingTimer> repeating_timer_;
  base::SequenceBound<DIPSStorage> storage_;

  base::WeakPtrFactory<DIPSService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_SERVICE_H_
