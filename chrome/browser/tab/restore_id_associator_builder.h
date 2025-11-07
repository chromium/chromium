// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_RESTORE_ID_ASSOCIATOR_BUILDER_H_
#define CHROME_BROWSER_TAB_RESTORE_ID_ASSOCIATOR_BUILDER_H_

#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/restore_id_associator.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

// Builds a `RestoreIdAssociator` by collecting persisted tab and collection
// state. This builder is used to construct the necessary data structures for
// the associator.
class RestoreIdAssociatorBuilder {
 public:
  virtual ~RestoreIdAssociatorBuilder() = default;

  // Registers the persisted state of a collection, including its children's
  // storage IDs. Builds the parent-child relationships between nodes.
  virtual void RegisterCollection(int storage_id,
                                  TabStorageType type,
                                  const tabs_pb::Children& children) = 0;

  // Registers the persisted state of a single tab, associating its storage ID
  // with its in-memory representation.
  virtual void RegisterTab(int storage_id,
                           const tabs_pb::TabState& tab_state) = 0;

  // Constructs and returns a `RestoreIdAssociator`.
  virtual std::unique_ptr<RestoreIdAssociator> BuildAssociator() = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_RESTORE_ID_ASSOCIATOR_BUILDER_H_
