// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_WEB_VIEW_DELEGATE_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_WEB_VIEW_DELEGATE_IMPL_H_

#include "ash/assistant/ui/assistant_web_view_delegate.h"
#include "base/macros.h"

namespace ash {

class AssistantWebViewDelegateImpl : public AssistantWebViewDelegate {
 public:
  AssistantWebViewDelegateImpl();
  ~AssistantWebViewDelegateImpl() override;

  // AssistantWebViewDelegate:
  void UpdateBackButtonVisibility(views::Widget* widget,
                                  bool visibility) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantWebViewDelegateImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_WEB_VIEW_DELEGATE_IMPL_H_
