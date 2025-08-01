// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_ADDRESS_POOL_MANAGER_TYPES_H_
#define PARTITION_ALLOC_ADDRESS_POOL_MANAGER_TYPES_H_

namespace partition_alloc::internal {

enum pool_handle : unsigned;

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_ADDRESS_POOL_MANAGER_TYPES_H_
