// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_CORE_SHARE_TARGETS_H_
#define CHROME_BROWSER_SHARE_CORE_SHARE_TARGETS_H_

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/share/proto/share_target.pb.h"

namespace sharing {

struct ShareTargetsSingletonTrait;
class ShareTargetsObserver;

class ShareTargets {
 public:
  ShareTargets(const ShareTargets&) = delete;
  ShareTargets& operator=(const ShareTargets&) = delete;
  virtual ~ShareTargets();

  static ShareTargets* GetInstance();  // Singleton

  // Update the internal list from a binary proto fetched from the network.
  // Same integrity checks apply. This can be called multiple times with new
  // protos.
  void PopulateFromDynamicUpdate(const std::string& binary_pb);

  // Observers -----------------------------------------------------------------

  // Adds/Removes an Observer.
  void AddObserver(ShareTargetsObserver* observer);
  void RemoveObserver(ShareTargetsObserver* observer);

 protected:
  // Creator must call one of Populate* before calling other methods.
  ShareTargets();

  // Used in metrics, do not reorder.
  enum class UpdateResult {
    SUCCESS = 1,
    FAILED_EMPTY = 2,
    FAILED_PROTO_PARSE = 3,
    FAILED_VERSION_CHECK = 4,
    SKIPPED_VERSION_CHECK_EQUAL = 5,
    kMaxValue = 5,
  };

  // Used in metrics, do not reorder.
  enum class UpdateOrigin {
    RESOURCE_BUNDLE = 1,
    DYNAMIC_UPDATE = 2,
  };

  // Visible for testing.
  std::string GetCountryStringFromID(int countryID);

 private:
  // Read data from an serialized protobuf and update the internal list
  // only if it passes integrity checks.
  UpdateResult PopulateFromBinaryPb(const std::string& binary_pb);

  // Read data from the main ResourceBundle. This updates the internal list
  // only if the data passes integrity checks. This is normally called once
  // after construction.
  void PopulateFromResourceBundle();

  // Record the result of an update attempt.
  virtual void RecordUpdateMetrics(UpdateResult result, UpdateOrigin src_name);

  // Swap in a different targets. This will rebuild file_type_by_ext_ index.
  void SwapTargetsLocked(std::unique_ptr<mojom::MapLocaleTargets>& new_targets);

  // The latest targets we've committed. Starts out null.
  // Protected by lock_.
  std::unique_ptr<mojom::MapLocaleTargets> targets_;

  mutable base::Lock lock_;

  // Observers ----------------------------------------------------------------

  // Notify all ShareTargetsObservers registered that the ShareTargets have been
  // updated.
  void NotifyObserver(ShareTargetsObserver* observer);
  void NotifyShareTargetUpdated();

  base::ObserverList<ShareTargetsObserver>::Unchecked observers_;

  FRIEND_TEST_ALL_PREFIXES(ShareTargetsTest, UnpackResourceBundle);
  FRIEND_TEST_ALL_PREFIXES(ShareTargetsTest, BadProto);
  FRIEND_TEST_ALL_PREFIXES(ShareTargetsTest, BadUpdateFromExisting);
  FRIEND_TEST_ALL_PREFIXES(ShareTargetsTest, CountryCodeMatches);

  friend struct ShareTargetsSingletonTrait;
};

}  // namespace sharing

#endif  // CHROME_BROWSER_SHARE_CORE_SHARE_TARGETS_H_
