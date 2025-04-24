// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_MODAL_MANAGER_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_MODAL_MANAGER_H_

#include <memory>

#include "ui/views/widget/widget.h"

namespace glic {

class GlicModalView;

class GlicModalManager {
 public:
  GlicModalManager();
  ~GlicModalManager();
  GlicModalManager(const GlicModalManager&) = delete;
  GlicModalManager& operator=(const GlicModalManager&) = delete;

  void ShowModal(std::u16string label, views::Widget* glic_widget);

 private:
  void CloseModal(views::Widget::ClosedReason reason);
  gfx::Rect GetModalBounds(views::Widget* glic_widget,
                           GlicModalView* modal_view);

  std::unique_ptr<views::Widget> modal_widget_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_MODAL_MANAGER_H_
