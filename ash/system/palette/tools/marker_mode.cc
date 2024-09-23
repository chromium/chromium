// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/tools/marker_mode.h"

#include "ash/annotator/annotation_source_watcher.h"
#include "ash/annotator/annotator_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_ids.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace {

aura::Window* GetRootWindow() {
  const int64_t display_id = Shell::Get()->cursor_manager()->GetDisplay().id();

  // The Display object returned by `CursorManager::GetDisplay()` may be stale,
  // but will have the correct id.
  auto* root = Shell::GetRootWindowForDisplayId(display_id);
  return root ? root : Shell::GetPrimaryRootWindow();
}

}  // namespace

MarkerMode::MarkerMode(Delegate* delegate) : CommonPaletteTool(delegate) {}

MarkerMode::~MarkerMode() = default;

PaletteGroup MarkerMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId MarkerMode::GetToolId() const {
  return PaletteToolId::MARKER_MODE;
}

// TODO(b/339834202): Consider changing to base class's OnEnable() calling a
// overridable function.
void MarkerMode::OnEnable() {
  CommonPaletteTool::OnEnable();
  delegate()->HidePaletteImmediately();
  Shell::Get()
      ->annotator_controller()
      ->annotation_source_watcher()
      ->NotifyMarkerEnabled(GetRootWindow());
}

// TODO(b/339834202): Consider changing to base class's OnDisable() calling a
// overridable function.
void MarkerMode::OnDisable() {
  CommonPaletteTool::OnDisable();
  Shell::Get()
      ->annotator_controller()
      ->annotation_source_watcher()
      ->NotifyMarkerDisabled();
}

views::View* MarkerMode::CreateView() {
  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_MARKER_MODE));
}

// TODO(b/339834202): Consider setting icons in ctor.
const gfx::VectorIcon& MarkerMode::GetActiveTrayIcon() const {
  return kPaletteTrayIconProjectorIcon;
}

const gfx::VectorIcon& MarkerMode::GetPaletteIcon() const {
  return kPaletteTrayIconProjectorIcon;
}

// TODO(b/339834202): Consider changing to base class's OnViewClicked() calling
// a overridable function.
void MarkerMode::OnViewClicked(views::View* sender) {
  Shell::Get()
      ->annotator_controller()
      ->annotation_source_watcher()
      ->NotifyMarkerClicked(GetRootWindow());
  CommonPaletteTool::OnViewClicked(sender);
}

}  // namespace ash
