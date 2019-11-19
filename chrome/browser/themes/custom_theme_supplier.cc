// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/custom_theme_supplier.h"

#include "base/memory/ref_counted_memory.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"

namespace {

// Creates a sequenced task runner to delete an instance of CustomThemeSupplier
// on.
scoped_refptr<base::SequencedTaskRunner> CreateTaskRunnerForDeletion() {
  return base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT});
}

}  // namespace

CustomThemeSupplier::CustomThemeSupplier(ThemeType theme_type)
    : base::RefCountedDeleteOnSequence<CustomThemeSupplier>(
          CreateTaskRunnerForDeletion()),
      theme_type_(theme_type) {}

CustomThemeSupplier::~CustomThemeSupplier() {}

void CustomThemeSupplier::StartUsingTheme() {}

void CustomThemeSupplier::StopUsingTheme() {}

bool CustomThemeSupplier::GetTint(int id, color_utils::HSL* hsl) const {
  return false;
}

bool CustomThemeSupplier::GetColor(int id, SkColor* color) const {
  return false;
}

bool CustomThemeSupplier::GetDisplayProperty(int id, int* result) const {
  return false;
}

gfx::Image CustomThemeSupplier::GetImageNamed(int id) {
  return gfx::Image();
}

base::RefCountedMemory* CustomThemeSupplier::GetRawData(
    int idr_id,
    ui::ScaleFactor scale_factor) const {
  return NULL;
}

bool CustomThemeSupplier::HasCustomImage(int id) const {
  return false;
}
