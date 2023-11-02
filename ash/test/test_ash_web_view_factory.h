// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_ASH_WEB_VIEW_FACTORY_H_
#define ASH_TEST_TEST_ASH_WEB_VIEW_FACTORY_H_

#include <memory>

#include "ash/public/cpp/ash_web_view_factory.h"

namespace ash {

// An implementation of AshWebViewFactory for use in unittests.
class TestAshWebViewFactory : public AshWebViewFactory {
 public:
  TestAshWebViewFactory();
  TestAshWebViewFactory(const TestAshWebViewFactory& copy) = delete;
  TestAshWebViewFactory& operator=(const TestAshWebViewFactory& assign) =
      delete;
  ~TestAshWebViewFactory() override;

  // AshWebViewFactory:
  std::unique_ptr<AshWebView> Create(
      const AshWebView::InitParams& params) override;
};

}  // namespace ash

#endif  // ASH_TEST_TEST_ASH_WEB_VIEW_FACTORY_H_
