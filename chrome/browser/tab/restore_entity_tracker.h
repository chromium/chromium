// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_RESTORE_ENTITY_TRACKER_H_
#define CHROME_BROWSER_TAB_RESTORE_ENTITY_TRACKER_H_

#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

class TabStateStorageDatabase;

// A callback invoked when a TabInterface is associated with its
// persistent storage_id.
using OnTabAssociation =
    base::RepeatingCallback<void(StorageId storage_id, const TabInterface*)>;

// A callback invoked when a TabCollection is associated with its
// persistent storage_id.
using OnCollectionAssociation =
    base::RepeatingCallback<void(StorageId storage_id, const TabCollection*)>;

// Associates in-memory nodes with their storage IDs in the storage layer.
class RestoreEntityTracker {
 public:
  virtual ~RestoreEntityTracker() = default;

  // Registers the persisted state of a collection, including its children's
  // storage IDs. Builds the parent-child relationships between nodes.
  virtual void RegisterCollection(StorageId storage_id,
                                  TabStorageType type,
                                  const tabs_pb::Children& children,
                                  base::PassKey<TabStateStorageDatabase>) = 0;

  // Registers the persisted state of a single tab, associating its storage ID
  // with its in-memory representation.
  virtual void RegisterTab(StorageId storage_id,
                           const tabs_pb::TabState& tab_state,
                           base::PassKey<TabStateStorageDatabase>) = 0;

  // Associates a tab and its ancestor TabCollections with their respective
  // storage IDs. Returns true if the tab was successfully associated.
  virtual bool AssociateTabAndAncestors(const TabInterface*) = 0;

  // Associates a PinnedTabCollection with its storage ID. This must be
  // associated separately since it may be empty and not contain any tabs.
  // Returns true if successfully associated.
  virtual void AssociatePinnedCollection(const PinnedTabCollection*) = 0;

  // Returns true if the collection has been associated.
  virtual bool HasCollectionBeenAssociated(TabCollection::Handle) = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_RESTORE_ENTITY_TRACKER_H_
