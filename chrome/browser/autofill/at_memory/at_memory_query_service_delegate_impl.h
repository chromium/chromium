// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service_delegate.h"

class Profile;

namespace autofill {

// Implementation of the AtMemoryQueryServiceDelegate interface.
// Provides browser-level services and contextual data retrieval to
// `AtMemoryQueryService`.
// TODO(crbug.com/525386262): Implement CreateDeviceAuthenticator() for use
// by `AtMemoryQueryService`.
class AtMemoryQueryServiceDelegateImpl : public AtMemoryQueryServiceDelegate {
 public:
  explicit AtMemoryQueryServiceDelegateImpl(Profile* profile);
  AtMemoryQueryServiceDelegateImpl(const AtMemoryQueryServiceDelegateImpl&) =
      delete;
  AtMemoryQueryServiceDelegateImpl& operator=(
      const AtMemoryQueryServiceDelegateImpl&) = delete;
  ~AtMemoryQueryServiceDelegateImpl() override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_DELEGATE_IMPL_H_
