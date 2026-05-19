// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PIN_CANDIDATE_PROVIDER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PIN_CANDIDATE_PROVIDER_H_

#include "chrome/browser/glic/host/glic.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace glic {

// Glic-internal interface for subscribing to pin candidates.
class GlicPinCandidateProvider {
 public:
  virtual ~GlicPinCandidateProvider() = default;

  // Subscribes to changes in pin candidates.
  virtual void SubscribeToPinCandidates(
      mojom::GetPinCandidatesOptionsPtr options,
      mojo::PendingRemote<mojom::PinCandidatesObserver> observer) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PIN_CANDIDATE_PROVIDER_H_
