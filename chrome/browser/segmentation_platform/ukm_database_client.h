// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATABASE_CLIENT_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATABASE_CLIENT_H_

#include <memory>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "chrome/browser/profiles/profile.h"
#include "components/segmentation_platform/internal/signals/ukm_observer.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"

namespace ukm {
class UkmRecorderImpl;
}

namespace segmentation_platform {
class UkmDataManager;
class UkmObserver;

// Provides UKM functionality to the segmentation platform service(s).
class UkmDatabaseClient {
 public:
  UkmDatabaseClient();
  ~UkmDatabaseClient();

  UkmDatabaseClient(const UkmDatabaseClient&) = delete;
  UkmDatabaseClient& operator=(const UkmDatabaseClient&) = delete;

  // Must be called before any profiles (segmentation services) are created.
  // `in_memory_database` is used in tests only to create in memory databases to
  // make tests faster.
  void PreProfileInit(bool in_memory_database);

  // Must be called after profiles are destroyed, but before metrics service is
  // destroyed.
  void PostMessageLoopRun();

  // UkmDataManager will be valid for the lifetime of all the profiles. It is
  // created before profiles are created at startup. It is safe to use this
  // pointer till ProfileManagerDestroying() is called.
  segmentation_platform::UkmDataManager* GetUkmDataManager();

  // UKM observer will use the test recorder to observe metrics.
  void set_ukm_recorder_for_testing(ukm::UkmRecorderImpl* ukm_recorder) {
    DCHECK(!ukm_observer_);
    ukm_recorder_for_testing_ = ukm_recorder;
  }

  UkmObserver* ukm_observer_for_testing() { return ukm_observer_.get(); }

  void TearDownForTesting();

 private:
  raw_ptr<ukm::UkmRecorderImpl> ukm_recorder_for_testing_;
  std::unique_ptr<UkmObserver> ukm_observer_;
  std::unique_ptr<UkmDataManager> ukm_data_manager_;
};

// Class to own the UkmDatabaseClient. Supports overriding instances in tests.
class UkmDatabaseClientHolder {
 public:
  // Always returns the main client instance, unless a client was set for
  // testing. Can be called with nullptr to get the main instance when `profile`
  // is not created.
  static UkmDatabaseClient& GetClientInstance(Profile* profile);

  // Sets or removes the instance used by `profile` for testing. Thread safe,
  // and GetClientInstance() will return the new client based on the profile.
  // Note that if GetClientInstance() is called with nullptr, the main instance
  // will still be returned.
  static void SetUkmClientForTesting(Profile* profile,
                                     UkmDatabaseClient* client);

  UkmDatabaseClientHolder(const UkmDatabaseClientHolder&) = delete;
  UkmDatabaseClientHolder& operator=(const UkmDatabaseClientHolder&) = delete;

 private:
  friend base::NoDestructor<UkmDatabaseClientHolder>;

  static UkmDatabaseClientHolder& GetInstance();
  UkmDatabaseClientHolder();
  ~UkmDatabaseClientHolder();

  void SetUkmClientForTestingInternal(Profile* profile,
                                      UkmDatabaseClient* client);

  base::Lock lock_;
  std::map<raw_ptr<Profile>, raw_ptr<UkmDatabaseClient>> clients_for_testing_
      GUARDED_BY(lock_);

  std::unique_ptr<UkmDatabaseClient> main_client_;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_UKM_DATABASE_CLIENT_H_
