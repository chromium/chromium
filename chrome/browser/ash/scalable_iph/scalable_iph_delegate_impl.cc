// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"

namespace ash {

ScalableIphDelegateImpl::ScalableIphDelegateImpl(Profile* profile)
    : profile_(profile) {}

// Remember NOT to interact with `iph_session` from the destructor. See the
// comment of `ScalableIphDelegate::ShowBubble` for details.
ScalableIphDelegateImpl::~ScalableIphDelegateImpl() = default;

void ScalableIphDelegateImpl::ShowBubble(
    const scalable_iph::ScalableIphDelegate::BubbleParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  // TODO(b/284158855): Add implementation.
}

void ScalableIphDelegateImpl::ShowNotification(
    const scalable_iph::ScalableIphDelegate::NotificationParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  // TODO(b/284158831): Add implementation.
}

}  // namespace ash
