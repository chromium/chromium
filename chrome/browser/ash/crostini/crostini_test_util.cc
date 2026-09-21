// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_test_util.h"

#include <memory>
#include <utility>

#include "chrome/test/views/chrome_test_views_delegate.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace {

// Owned. Created by SetUpViewsEnvironmentForTesting() and destroyed by
// TearDownViewsEnvironmentForTesting().
views::ScopedViewsTestHelper* g_views_helper = nullptr;

// ViewsDelegate to provide context to dialog creation functions which do not
// allow InitParams to be set, and pass a null |context| argument to
// DialogDelegate::CreateDialogWidget().
class TestViewsDelegateWithContext : public ChromeTestViewsDelegate<> {
 public:
  TestViewsDelegateWithContext() = default;

  TestViewsDelegateWithContext(const TestViewsDelegateWithContext&) = delete;
  TestViewsDelegateWithContext& operator=(const TestViewsDelegateWithContext&) =
      delete;

  void set_context(gfx::NativeWindow context) { context_ = context; }

  // ViewsDelegate:
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {
    if (!params->context) {
      params->context = context_;
    }
    ChromeTestViewsDelegate::OnBeforeWidgetInit(params, delegate);
  }

 private:
  gfx::NativeWindow context_ = gfx::NativeWindow();
};

}  // namespace

namespace crostini {

void SetUpViewsEnvironmentForTesting() {
  auto views_delegate = std::make_unique<TestViewsDelegateWithContext>();
  TestViewsDelegateWithContext* views_delegate_weak = views_delegate.get();
  g_views_helper = new views::ScopedViewsTestHelper(std::move(views_delegate));
  views_delegate_weak->set_context(g_views_helper->GetContext());
}

void TearDownViewsEnvironmentForTesting() {
  delete g_views_helper;
  g_views_helper = nullptr;
}

}  // namespace crostini
