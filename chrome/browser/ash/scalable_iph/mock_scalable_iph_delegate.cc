// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/mock_scalable_iph_delegate.h"

namespace ash {
namespace test {

MockScalableIphDelegate::MockScalableIphDelegate() = default;
MockScalableIphDelegate::~MockScalableIphDelegate() = default;

void MockScalableIphDelegate::SetDelegate(
    std::unique_ptr<scalable_iph::ScalableIphDelegate> delegate) {
  CHECK(!delegate_) << "You are NOT allowed to set a delegate object twice";

  delegate_ = std::move(delegate);
}

void MockScalableIphDelegate::FakeObservers() {
  CHECK(delegate_) << "Delegate must be set to enable fake behaviors";
  CHECK(!observers_fake_enabled_) << "Fake is already set for observers";
  observers_fake_enabled_ = true;

  ON_CALL(*this, AddObserver).WillByDefault([this](Observer* observer) {
    delegate_->AddObserver(observer);
  });

  ON_CALL(*this, RemoveObserver).WillByDefault([this](Observer* observer) {
    delegate_->RemoveObserver(observer);
  });

  ON_CALL(*this, IsOnline).WillByDefault([this]() {
    return delegate_->IsOnline();
  });
}

void MockScalableIphDelegate::FakeClientAgeInDays() {
  CHECK(delegate_) << "Delegate must be set to enable fake behaviors";
  CHECK(!client_age_fake_enabled_) << "Fake is already set for client age";
  client_age_fake_enabled_ = true;

  ON_CALL(*this, ClientAgeInDays).WillByDefault([this]() {
    return delegate_->ClientAgeInDays();
  });
}

void MockScalableIphDelegate::FakeShowBubble() {
  CHECK(delegate_) << "Delegate must be set to enable fake behaviors";
  CHECK(!show_bubble_fake_enabled_)
      << "Fake is already set for showing a bubble";
  show_bubble_fake_enabled_ = true;

  ON_CALL(*this, ShowBubble)
      .WillByDefault(
          [this](const scalable_iph::ScalableIphDelegate::BubbleParams& params,
                 std::unique_ptr<scalable_iph::IphSession> iph_session) {
            return delegate_->ShowBubble(params, std::move(iph_session));
          });
}

void MockScalableIphDelegate::FakeShowNotification() {
  CHECK(delegate_) << "Delegate must be set to enable fake behaviors";
  CHECK(!show_notification_fake_enabled_)
      << "Fake is already set for showing notifications";
  show_notification_fake_enabled_ = true;

  ON_CALL(*this, ShowNotification)
      .WillByDefault(
          [this](const scalable_iph::ScalableIphDelegate::NotificationParams&
                     params,
                 std::unique_ptr<scalable_iph::IphSession> iph_session) {
            return delegate_->ShowNotification(params, std::move(iph_session));
          });
}

void MockScalableIphDelegate::FakePerformActionForScalableIph() {
  CHECK(delegate_) << "Delegate must be set to enable fake behaviors";
  CHECK(!perform_action_for_scalable_iph_enabled_)
      << "Fake is already set for performing an action for Scalable Iph";
  perform_action_for_scalable_iph_enabled_ = true;

  ON_CALL(*this, PerformActionForScalableIph)
      .WillByDefault([this](scalable_iph::ActionType action_type) {
        return delegate_->PerformActionForScalableIph(action_type);
      });
}

}  // namespace test
}  // namespace ash
