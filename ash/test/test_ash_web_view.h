// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_ASH_WEB_VIEW_H_
#define ASH_TEST_TEST_ASH_WEB_VIEW_H_

#include "ash/public/cpp/ash_web_view.h"
#include "base/observer_list.h"

namespace views {
class View;
}  // namespace views
namespace ash {

// An implementation of AshWebView for use in unittests.
class TestAshWebView : public AshWebView {
 public:
  TestAshWebView();
  ~TestAshWebView() override;

  TestAshWebView(const TestAshWebView&) = delete;
  TestAshWebView& operator=(const TestAshWebView&) = delete;

  // AshWebView:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  gfx::NativeView GetNativeView() override;
  bool GoBack() override;
  void Navigate(const GURL& url) override;
  views::View* GetInitiallyFocusedView() override;
  void RequestFocus() override;
  bool HasFocus() const override;

 private:
  base::ObserverList<Observer> observers_;
  bool focused_ = false;

  base::WeakPtrFactory<TestAshWebView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_TEST_TEST_ASH_WEB_VIEW_H_
