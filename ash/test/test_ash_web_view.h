// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_ASH_WEB_VIEW_H_
#define ASH_TEST_TEST_ASH_WEB_VIEW_H_

#include "ash/public/cpp/ash_web_view.h"
#include "base/observer_list.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "url/gurl.h"

namespace views {
class View;
}  // namespace views

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace ash {

// An implementation of AshWebView for use in unittests.
class TestAshWebView : public AshWebView {
  METADATA_HEADER(TestAshWebView, AshWebView)

 public:
  explicit TestAshWebView(const AshWebView::InitParams& init_params);
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
  const GURL& GetVisibleURL() override;
  bool IsErrorDocument() override;
  void SetCornerRadii(const gfx::RoundedCornersF& corner_radii) override;
  const base::UnguessableToken& GetMediaSessionRequestId() override;

  const AshWebView::InitParams& init_params_for_testing() const {
    return init_params_;
  }

  // The most recent url that was requested via Navigate(). Will be empty if
  // Navigate() has not been called.
  const GURL& current_url() const { return current_url_; }

  void set_is_error_document(bool is_error_document) {
    is_error_document_ = is_error_document;
  }

 private:
  base::ObserverList<Observer> observers_;
  bool focused_ = false;
  AshWebView::InitParams init_params_;
  GURL current_url_;
  bool is_error_document_ = false;

  base::WeakPtrFactory<TestAshWebView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_TEST_TEST_ASH_WEB_VIEW_H_
