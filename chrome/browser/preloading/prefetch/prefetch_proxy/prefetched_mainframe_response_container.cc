// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"

PrefetchedMainframeResponseContainer::PrefetchedMainframeResponseContainer(
    const net::IsolationInfo& isolation_info,
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body)
    : isolation_info_(isolation_info),
      head_(std::move(head)),
      body_(std::move(body)) {}

PrefetchedMainframeResponseContainer::~PrefetchedMainframeResponseContainer() =
    default;

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchedMainframeResponseContainer::Clone() const {
  return std::make_unique<PrefetchedMainframeResponseContainer>(
      isolation_info_, head_->Clone(), std::make_unique<std::string>(*body_));
}

network::mojom::URLResponseHeadPtr
PrefetchedMainframeResponseContainer::TakeHead() {
  DCHECK(head_);
  return std::move(head_);
}

std::unique_ptr<std::string> PrefetchedMainframeResponseContainer::TakeBody() {
  DCHECK(body_);
  return std::move(body_);
}
