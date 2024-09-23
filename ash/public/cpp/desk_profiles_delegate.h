// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESK_PROFILES_DELEGATE_H_
#define ASH_PUBLIC_CPP_DESK_PROFILES_DELEGATE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// This struct provides a summary of a profile that exists in Lacros.
struct ASH_PUBLIC_EXPORT LacrosProfileSummary {
  LacrosProfileSummary();
  LacrosProfileSummary(const LacrosProfileSummary&);
  LacrosProfileSummary(LacrosProfileSummary&&);
  LacrosProfileSummary& operator=(const LacrosProfileSummary&);
  LacrosProfileSummary& operator=(LacrosProfileSummary&&);

  uint64_t profile_id = 0;

  // Profile name.
  std::u16string name;

  // Profile email, may be empty.
  std::u16string email;

  // Profile icon.
  gfx::ImageSkia icon;
};

// This interface provides profile information from Lacros to clients in Ash. It
// is implemented by `DeskProfilesAsh`, which in turn receives profile
// information from its counterpart on the Lacros side: `DeskProfilesLacros`. It
// allows a client to get a snapshot of current profiles as well as observe
// profile modifications.
class ASH_PUBLIC_EXPORT DeskProfilesDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a profile is inserted or updated.
    virtual void OnProfileUpsert(const LacrosProfileSummary& summary) {}
    // Called when a profile is removed.
    virtual void OnProfileRemoved(uint64_t profile_id) {}
  };

  virtual ~DeskProfilesDelegate() = default;

  // Returns a snapshot of the current profiles.
  virtual const std::vector<LacrosProfileSummary>& GetProfilesSnapshot()
      const = 0;

  // Returns the snapshot of profile by giving profile id.
  virtual const LacrosProfileSummary* GetProfilesSnapshotByProfileId(
      uint64_t profile_id) const = 0;

  // Returns the primary profile ID.
  virtual uint64_t GetPrimaryProfileId() const = 0;

  // If `profile_id` is zero, this returns `GetPrimaryProfileId`, otherwise
  // `profile_id` is returned as-is.
  uint64_t ResolveProfileId(uint64_t profile_id);

  // Adds or removes an observer that will receive profile updates.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESK_PROFILES_DELEGATE_H_
