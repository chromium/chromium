// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_PROFILE_DISCARD_OPT_OUT_LIST_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_PROFILE_DISCARD_OPT_OUT_LIST_HELPER_H_

#include <map>
#include <string>

#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace performance_manager::user_tuning {

// This class observes changes to each Profile's Discard Opt Out List
// preference, and updates the PageDiscardingHelper accordingly.
class ProfileDiscardOptOutListHelper {
 public:
  // Subclasses of this `Delegate` can be used in tests to observe the effects
  // of `ProfileDiscardOptOutListHelper` without instantiating a
  // `PageDiscardingHelper`.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void ClearPatterns(const std::string& browser_context_id) = 0;
    virtual void SetPatterns(const std::string& browser_context_id,
                             const std::vector<std::string>& patterns) = 0;
  };

  // If `delegate` is null, a default implementation that sets patterns on the
  // `PageDiscardingHelper` instance is used.
  explicit ProfileDiscardOptOutListHelper(
      std::unique_ptr<Delegate> delegate = nullptr);
  ~ProfileDiscardOptOutListHelper();

  void OnProfileAdded(Profile* profile);
  void OnProfileWillBeRemoved(Profile* profile);

 private:
  friend class ProfileDiscardOptOutListHelperTest;
  // A helper class to encapsulate the tracking of a single profile's
  // preference.
  class ProfileDiscardOptOutTracker {
   public:
    // Initialize this tracker's state, such that it starts observing the
    // relevant prefs in `pref_service`.
    ProfileDiscardOptOutTracker(const std::string& browser_context_id,
                                PrefService* pref_service,
                                Delegate* delegate);
    ~ProfileDiscardOptOutTracker();

   private:
    void OnOptOutListChanged();

    const std::string browser_context_id_;
    PrefChangeRegistrar pref_change_registrar_;
    const raw_ptr<Delegate> delegate_;
  };

  // These get their arguments from the public versions above (that take a
  // Profile*). Splitting them off here allows unit testing without creating an
  // entire Profile and all the associated components.
  void OnProfileAddedImpl(const std::string& browser_context_id,
                          PrefService* pref_service);
  void OnProfileWillBeRemovedImpl(const std::string& browser_context_id);

  const std::unique_ptr<Delegate> delegate_;

  std::map<std::string, ProfileDiscardOptOutTracker> discard_opt_out_trackers_;
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_PROFILE_DISCARD_OPT_OUT_LIST_HELPER_H_
