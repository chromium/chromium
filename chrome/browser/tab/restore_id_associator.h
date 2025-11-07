// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_RESTORE_ID_ASSOCIATOR_H_
#define CHROME_BROWSER_TAB_RESTORE_ID_ASSOCIATOR_H_

#include "base/functional/callback.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// A callback invoked when a TabInterface is associated with its
// persistent storage_id.
using OnTabAssociation =
    base::RepeatingCallback<void(int storage_id, const TabInterface*)>;

// A callback invoked when a TabCollection is associated with its
// persistent storage_id.
using OnCollectionAssociation =
    base::RepeatingCallback<void(int storage_id, const TabCollection*)>;

// Associates in-memory nodes with their storage IDs in the storage layer.
class RestoreIdAssociator {
 public:
  virtual ~RestoreIdAssociator() = default;

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

#endif  // CHROME_BROWSER_TAB_RESTORE_ID_ASSOCIATOR_H_
