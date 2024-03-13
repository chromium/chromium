// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCALABLE_IPH_MOCK_SCALABLE_IPH_DELEGATE_H_
#define CHROME_BROWSER_ASH_SCALABLE_IPH_MOCK_SCALABLE_IPH_DELEGATE_H_

#include <memory>

#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace test {

class MockScalableIphDelegate : public scalable_iph::ScalableIphDelegate {
 public:
  MockScalableIphDelegate();
  ~MockScalableIphDelegate() override;

  MOCK_METHOD(bool,
              ShowBubble,
              (const scalable_iph::ScalableIphDelegate::BubbleParams& params,
               std::unique_ptr<scalable_iph::IphSession> iph_session),
              (override));
  MOCK_METHOD(
      bool,
      ShowNotification,
      (const scalable_iph::ScalableIphDelegate::NotificationParams& params,
       std::unique_ptr<scalable_iph::IphSession> iph_session),
      (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(bool, IsOnline, (), (override));
  MOCK_METHOD(int, ClientAgeInDays, (), (override));
  MOCK_METHOD(void,
              PerformActionForScalableIph,
              (scalable_iph::ActionType action_type),
              (override));

  // Specify a delegate object and enable fake behaviors. We will want a fake
  // behavior for the most of cases, i.e. Unlike `ShowBubble` etc, we won't test
  // `IsOnline` called but we simulate/fake events/states. For now, we simply
  // use a real object as `ScalableIphDelegateImpl` works as a fake easily for
  // now and it's an easy way to increases test coverage.
  void SetDelegate(std::unique_ptr<scalable_iph::ScalableIphDelegate> delegate);
  scalable_iph::ScalableIphDelegate* fake_delegate() { return delegate_.get(); }
  void FakeObservers();
  void FakeClientAgeInDays();
  void FakeShowBubble();
  void FakeShowNotification();
  void FakePerformActionForScalableIph();

 private:
  std::unique_ptr<scalable_iph::ScalableIphDelegate> delegate_;
  bool observers_fake_enabled_ = false;
  bool client_age_fake_enabled_ = false;
  bool show_bubble_fake_enabled_ = false;
  bool show_notification_fake_enabled_ = false;
  bool perform_action_for_scalable_iph_enabled_ = false;
};

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCALABLE_IPH_MOCK_SCALABLE_IPH_DELEGATE_H_
