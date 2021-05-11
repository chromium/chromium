// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_test_util.h"

#include "chrome/test/views/chrome_test_views_delegate.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace {

std::unique_ptr<views::ScopedViewsTestHelper> views_helper_;

// ViewsDelegate to provide context to dialog creation functions which do not
// allow InitParams to be set, and pass a null |context| argument to
// DialogDelegate::CreateDialogWidget().
class TestViewsDelegateWithContext : public ChromeTestViewsDelegate<> {
 public:
  TestViewsDelegateWithContext() = default;

  void set_context(gfx::NativeWindow context) { context_ = context; }

  // ViewsDelegate:
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {
    if (!params->context)
      params->context = context_;
    ChromeTestViewsDelegate::OnBeforeWidgetInit(params, delegate);
  }

 private:
  gfx::NativeWindow context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestViewsDelegateWithContext);
};

}  // namespace

namespace crostini {

void SetUpViewsEnvironmentForTesting() {
  auto views_delegate = std::make_unique<TestViewsDelegateWithContext>();
  TestViewsDelegateWithContext* views_delegate_weak = views_delegate.get();
  views_helper_ =
      std::make_unique<views::ScopedViewsTestHelper>(std::move(views_delegate));
  views_delegate_weak->set_context(views_helper_->GetContext());
}

void TearDownViewsEnvironmentForTesting() {
  views_helper_.reset();
}

}  // namespace crostini
