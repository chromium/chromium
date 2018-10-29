// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_service_impl.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

BadgeServiceImpl::BadgeServiceImpl() = default;
BadgeServiceImpl::~BadgeServiceImpl() = default;

// static
void BadgeServiceImpl::Create(blink::mojom::BadgeServiceRequest request) {
  mojo::MakeStrongBinding(std::make_unique<BadgeServiceImpl>(),
                          std::move(request));
}

void BadgeServiceImpl::SetBadge() {
  NOTIMPLEMENTED();
}

void BadgeServiceImpl::ClearBadge() {
  NOTIMPLEMENTED();
}
