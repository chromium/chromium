// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RESTORE_ID_ASSOCIATOR_BUILDER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_RESTORE_ID_ASSOCIATOR_BUILDER_ANDROID_H_

#include <optional>

#include "chrome/browser/android/restore_id_associator_android.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/restore_id_associator.h"
#include "chrome/browser/tab/restore_id_associator_builder.h"
#include "chrome/browser/tab/tab_storage_type.h"

namespace tabs {

// Maps storage IDs to TabCollection and TabInterface nodes.
class RestoreIdAssociatorBuilderAndroid : public RestoreIdAssociatorBuilder {
 public:
  using RestoreIdAssociatorState =
      RestoreIdAssociatorAndroid::RestoreIdAssociatorState;

  RestoreIdAssociatorBuilderAndroid(OnTabAssociation, OnCollectionAssociation);
  ~RestoreIdAssociatorBuilderAndroid() override;

  void RegisterCollection(int storage_id,
                          TabStorageType type,
                          const tabs_pb::Children& children) override;
  void RegisterTab(int storage_id, const tabs_pb::TabState& tab_state) override;

  std::unique_ptr<RestoreIdAssociator> BuildAssociator() override;

 private:
  std::unique_ptr<RestoreIdAssociatorState> state_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_RESTORE_ID_ASSOCIATOR_BUILDER_ANDROID_H_
