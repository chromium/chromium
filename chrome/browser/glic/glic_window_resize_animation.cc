// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_resize_animation.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/glic_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace glic {

GlicWindowResizeAnimation::GlicWindowResizeAnimation(
    views::Widget* widget,
    GlicView* view,
    gfx::Size new_size,
    base::TimeDelta duration,
    FinishedCallback finished_callback)
    : finished_callback_(std::move(finished_callback)) {
  widget->SetSize(new_size);
  view->web_view()->SetSize(new_size);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&GlicWindowResizeAnimation::Finished,
                                weak_ptr_factory_.GetWeakPtr()));
}

GlicWindowResizeAnimation::~GlicWindowResizeAnimation() = default;

void GlicWindowResizeAnimation::Finished() {
  // Destroys `this`.
  std::move(finished_callback_).Run();
}

}  // namespace glic
