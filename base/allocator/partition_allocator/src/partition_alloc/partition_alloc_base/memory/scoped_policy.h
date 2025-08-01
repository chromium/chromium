// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_MEMORY_SCOPED_POLICY_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_MEMORY_SCOPED_POLICY_H_

namespace partition_alloc::internal::base::scoped_policy {

// Defines the ownership policy for a scoped object.
enum OwnershipPolicy {
  // The scoped object takes ownership of an object by taking over an existing
  // ownership claim.
  ASSUME,

  // The scoped object will retain the object and any initial ownership is
  // not changed.
  RETAIN
};

}  // namespace partition_alloc::internal::base::scoped_policy

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_MEMORY_SCOPED_POLICY_H_
