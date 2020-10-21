// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/prefetched_response_container.h"

PrefetchedResponseContainer::PrefetchedResponseContainer(
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body)
    : head_(std::move(head)), body_(std::move(body)) {}

PrefetchedResponseContainer::~PrefetchedResponseContainer() = default;

std::unique_ptr<PrefetchedResponseContainer>
PrefetchedResponseContainer::Clone() const {
  return std::make_unique<PrefetchedResponseContainer>(
      head_->Clone(), std::make_unique<std::string>(*body_));
}

network::mojom::URLResponseHeadPtr PrefetchedResponseContainer::TakeHead() {
  DCHECK(head_);
  return std::move(head_);
}

std::unique_ptr<std::string> PrefetchedResponseContainer::TakeBody() {
  DCHECK(body_);
  return std::move(body_);
}
