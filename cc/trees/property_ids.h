// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_IDS_H_
#define CC_TREES_PROPERTY_IDS_H_

namespace cc {

// Ids of property tree nodes, starting from 0.
enum {
  kInvalidPropertyNodeId = -1,
  kRootPropertyNodeId = 0,
  kSecondaryRootPropertyNodeId = 1,
  kContentsRootPropertyNodeId = kSecondaryRootPropertyNodeId,
  kViewportPropertyNodeId = kSecondaryRootPropertyNodeId
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_IDS_H_
