// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/at_memory/at_memory_query_service_delegate_impl.h"

namespace autofill {

AtMemoryQueryServiceDelegateImpl::AtMemoryQueryServiceDelegateImpl(
    Profile* profile)
    : profile_(profile) {}

AtMemoryQueryServiceDelegateImpl::~AtMemoryQueryServiceDelegateImpl() = default;

}  // namespace autofill
