// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_DELEGATE_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_DELEGATE_H_

#include "base/component_export.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

// A delegate of web container views in assistant/ui.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantWebViewDelegate {
 public:
  virtual ~AssistantWebViewDelegate() = default;

  // Updates the visibility of the back button in Assistant web container.
  virtual void UpdateBackButtonVisibility(views::Widget* widget,
                                          bool visibility) = 0;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_DELEGATE_H_
