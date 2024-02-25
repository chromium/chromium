// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/test/glanceables_test_new_window_delegate.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "url/gurl.h"

namespace ash {

class GlanceablesTestNewWindowDelegateImpl : public TestNewWindowDelegate {
 public:
  // TestNewWindowDelegate:
  void OpenUrl(const GURL& url,
               OpenUrlFrom from,
               Disposition disposition) override {
    last_opened_url_ = url;
  }

  GURL last_opened_url() const { return last_opened_url_; }

 private:
  GURL last_opened_url_;
};

GlanceablesTestNewWindowDelegate::GlanceablesTestNewWindowDelegate() {
  auto new_window_delegate =
      std::make_unique<GlanceablesTestNewWindowDelegateImpl>();
  new_window_delegate_ = new_window_delegate.get();
  new_window_delegate_provider_ =
      std::make_unique<TestNewWindowDelegateProvider>(
          std::move(new_window_delegate));
}

GlanceablesTestNewWindowDelegate::~GlanceablesTestNewWindowDelegate() = default;

GURL GlanceablesTestNewWindowDelegate::GetLastOpenedUrl() const {
  return new_window_delegate_->last_opened_url();
}

}  // namespace ash
