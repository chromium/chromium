// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_GRADUATION_GRADUATION_MANAGER_H_
#define ASH_PUBLIC_CPP_GRADUATION_GRADUATION_MANAGER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace content {
class BrowserContext;
class StoragePartition;
class StoragePartitionConfig;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::graduation {

// A checked observer which receives notification of changes to the
// Graduation app.
class ASH_PUBLIC_EXPORT GraduationManagerObserver
    : public base::CheckedObserver {
 public:
  // Invoked when the session active state is changed.
  virtual void OnGraduationAppUpdate(bool enabled) = 0;
};

// Creates interface to access browser-side functionalities in
// GraduationManagerImpl.
class ASH_PUBLIC_EXPORT GraduationManager {
 public:
  static GraduationManager* Get();

  GraduationManager();
  GraduationManager(const GraduationManager&) = delete;
  GraduationManager& operator=(const GraduationManager&) = delete;
  virtual ~GraduationManager();

  // Returns the language code of the device's current locale.
  virtual std::string GetLanguageCode() const = 0;

  // Returns identity manager for given `context`.
  // Needed to avoid ash/chrome dependency.
  virtual signin::IdentityManager* GetIdentityManager(
      content::BrowserContext* context) = 0;

  //  Returns storage partition for a given `context` and
  //  `storage_partition_config`.
  virtual content::StoragePartition* GetStoragePartition(
      content::BrowserContext* context,
      const content::StoragePartitionConfig& storage_partition_config) = 0;

  // Adds the specified observer to be notified of updates to the Graduation
  // app.
  virtual void AddObserver(GraduationManagerObserver* observer) = 0;
  virtual void RemoveObserver(GraduationManagerObserver* observer) = 0;

  // Used by browser tests to set and fast-forward the system time.
  virtual void SetClocksForTesting(const base::Clock* clock,
                                   const base::TickClock* tick_clock) = 0;

  // Used by browser tests to resume the timer after it is paused (e.g. during
  // fast-forwarding).
  virtual void ResumeTimerForTesting() = 0;
};

}  // namespace ash::graduation

#endif  // ASH_PUBLIC_CPP_GRADUATION_GRADUATION_MANAGER_H_
