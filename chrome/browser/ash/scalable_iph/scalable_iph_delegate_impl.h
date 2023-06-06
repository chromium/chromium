// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_IMPL_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {

class ScalableIphDelegateImpl : public scalable_iph::ScalableIphDelegate {
 public:
  explicit ScalableIphDelegateImpl(Profile* profile);
  ~ScalableIphDelegateImpl() override;

  // scalable_iph::ScalableIphDelegate:
  void ShowBubble(
      const BubbleParams& params,
      std::unique_ptr<scalable_iph::IphSession> iph_session) override;
  void ShowNotification(
      const NotificationParams& params,
      std::unique_ptr<scalable_iph::IphSession> iph_session) override;

 private:
  raw_ptr<Profile> profile_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCALABLE_IPH_SCALABLE_IPH_DELEGATE_IMPL_H_
