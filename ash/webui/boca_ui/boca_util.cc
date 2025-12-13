// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_util.h"

#include "ash/annotator/annotation_source_watcher.h"
#include "ash/annotator/annotator_controller.h"
#include "ash/shell.h"

namespace ash::boca::util {
void EnableOrDisableMarkerMode(bool enable) {
  ash::AnnotationSourceWatcher* annotator_source_watcher =
      Shell::Get()->annotator_controller()->annotation_source_watcher();

  if (enable) {
    annotator_source_watcher->NotifyMarkerEnabled(
        ash::Shell::GetPrimaryRootWindow());
  } else {
    annotator_source_watcher->NotifyMarkerDisabled();
  }
}
}  // namespace ash::boca::util
