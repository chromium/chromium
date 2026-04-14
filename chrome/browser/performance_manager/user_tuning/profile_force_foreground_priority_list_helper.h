// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_PROFILE_FORCE_FOREGROUND_PRIORITY_LIST_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_PROFILE_FORCE_FOREGROUND_PRIORITY_LIST_HELPER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class Profile;
class PrefService;

namespace performance_manager::user_tuning {

// This class observes changes to each Profile's Force Foreground Priority
// preferences, and updates the ForceForegroundVoterForOrigins accordingly.
class ProfileForceForegroundPriorityListHelper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void SetPatterns(const std::string& browser_context_id,
                             const base::ListValue& patterns) = 0;
    virtual void ClearPatterns(const std::string& browser_context_id) = 0;
  };

  explicit ProfileForceForegroundPriorityListHelper(
      std::unique_ptr<Delegate> delegate = nullptr);
  ~ProfileForceForegroundPriorityListHelper();

  void OnProfileAdded(Profile* profile);
  void OnProfileWillBeRemoved(Profile* profile);

  // These get their arguments from the public versions above (that take a
  // Profile*). Splitting them off here allows unit testing without creating an
  // entire Profile and all the associated components.
  void OnProfileAddedImpl(const std::string& browser_context_id,
                          PrefService* pref_service);
  void OnProfileWillBeRemovedImpl(const std::string& browser_context_id);

 private:
  // A helper class to encapsulate the tracking of a single profile's
  // preference.
  class ProfileForceForegroundPriorityTracker {
   public:
    ProfileForceForegroundPriorityTracker(const std::string& browser_context_id,
                                          PrefService* pref_service,
                                          Delegate* delegate);
    ~ProfileForceForegroundPriorityTracker();

   private:
    void OnPrefChanged();

    const std::string browser_context_id_;
    PrefChangeRegistrar pref_change_registrar_;
    const raw_ptr<Delegate> delegate_;
    // The last value of the preference. This is used to deduplicate updates. It
    // is an optional so that the first update is always sent to the delegate,
    // even if the preference is currently an empty list.
    std::optional<base::ListValue> last_origins_value_;
  };

  std::unique_ptr<Delegate> delegate_;

  absl::flat_hash_map<std::string,
                      std::unique_ptr<ProfileForceForegroundPriorityTracker>>
      trackers_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_PROFILE_FORCE_FOREGROUND_PRIORITY_LIST_HELPER_H_
