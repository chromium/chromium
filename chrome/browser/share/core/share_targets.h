// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_CORE_SHARE_TARGETS_H_
#define CHROME_BROWSER_SHARE_CORE_SHARE_TARGETS_H_

#include "chrome/browser/share/proto/share_target.pb.h"

#include "base/synchronization/lock.h"

namespace sharing {

struct ShareTargetsSingletonTrait;

class ShareTargets {
 public:
  virtual ~ShareTargets();

  static ShareTargets* GetInstance();  // Singleton

  // Update the internal list from a binary proto fetched from the network.
  // Same integrity checks apply. This can be called multiple times with new
  // protos.
  void PopulateFromDynamicUpdate(const std::string& binary_pb);

  //
  // Accessors
  //

  // Return the ShareTarget for a given locale. If the locale is not found
  // return a global locale.
  const mojom::ShareTargets* GetShareTargetsForLocale(
      const std::string& locale);

 protected:
  // Creator must call one of Populate* before calling other methods.
  ShareTargets();

 private:
  // Used in metrics, do not reorder.
  enum class UpdateResult {
    SUCCESS = 1,
    FAILED_EMPTY = 2,
    FAILED_PROTO_PARSE = 3,
    FAILED_VERSION_CHECK = 4,
    SKIPPED_VERSION_CHECK_EQUAL = 5,
  };

  // Read data from an serialized protobuf and update the internal list
  // only if it passes integrity checks.
  UpdateResult PopulateFromBinaryPb(const std::string& binary_pb);

  // Record the result of an update attempt.
  void RecordUpdateMetrics(UpdateResult result, const std::string& src_name);

  // Swap in a different targets. This will rebuild file_type_by_ext_ index.
  void SwapTargetsLocked(std::unique_ptr<mojom::MapLocaleTargets>& new_targets);

  // The latest targets we've committed. Starts out null.
  // Protected by lock_.
  std::unique_ptr<mojom::MapLocaleTargets> targets_;

  mutable base::Lock lock_;

  friend struct ShareTargetsSingletonTrait;
};

}  // namespace sharing

#endif  // CHROME_BROWSER_SHARE_CORE_SHARE_TARGETS_H_
