// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_RESULT_VIEW_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_RESULT_VIEW_H_

#include <memory>

#include "base/time/time.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace aura {
class Window;
}

namespace base {
class OneShotTimer;
}

namespace ui {
class Layer;
}

namespace ash {

enum class HighlighterGestureType;

// HighlighterResultView displays an animated shape that represents
// the result of the selection.
class HighlighterResultView : public views::View {
 public:
  HighlighterResultView();

  HighlighterResultView(const HighlighterResultView&) = delete;
  HighlighterResultView& operator=(const HighlighterResultView&) = delete;

  ~HighlighterResultView() override;

  static views::UniqueWidgetPtr Create(aura::Window* root_window);

  void Animate(const gfx::RectF& bounds,
               HighlighterGestureType gesture_type,
               base::OnceClosure done);

 private:
  void FadeIn(const base::TimeDelta& duration, base::OnceClosure done);
  void FadeOut(base::OnceClosure done);

  std::unique_ptr<ui::Layer> result_layer_;
  std::unique_ptr<base::OneShotTimer> animation_timer_;
};

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_RESULT_VIEW_H_
