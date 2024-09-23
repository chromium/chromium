// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_metrics.h"

namespace ash {

std::ostream& operator<<(std::ostream& out, WindowSnapActionSource source) {
  switch (source) {
    case WindowSnapActionSource::kNotSpecified:
      return out << "NotSpecified";
    case WindowSnapActionSource::kDragWindowToEdgeToSnap:
      return out << "DragWindowToEdgeToSnap";
    case WindowSnapActionSource::kLongPressCaptionButtonToSnap:
      return out << "LongPressCaptionButtonToSnap";
    case WindowSnapActionSource::kKeyboardShortcutToSnap:
      return out << "KeyboardShortcutToSnap";
    case WindowSnapActionSource::kDragOrSelectOverviewWindowToSnap:
      return out << "DragOrSelectOverviewWindowToSnap";
    case WindowSnapActionSource::kLongPressOverviewButtonToSnap:
      return out << "LongPressOverviewButtonToSnap";
    case WindowSnapActionSource::kDragUpFromShelfToSnap:
      return out << "DragUpFromShelfToSnap";
    case WindowSnapActionSource::kDragDownFromTopToSnap:
      return out << "DragDownFromTopToSnap";
    case WindowSnapActionSource::kDragTabToSnap:
      return out << "DragTabToSnap";
    case WindowSnapActionSource::kAutoSnapInSplitView:
      return out << "AutoSnapInSplitView";
    case WindowSnapActionSource::kSnapByWindowStateRestore:
      return out << "SnapByWindowStateRestore";
    case WindowSnapActionSource::kSnapByWindowLayoutMenu:
      return out << "SnapByWindowLayoutMenu";
    case WindowSnapActionSource::kSnapByFullRestoreOrDeskTemplateOrSavedDesk:
      return out << "SnapByFullRestoreOrDeskTemplateOrSavedDesk";
    case WindowSnapActionSource::kSnapByClamshellTabletTransition:
      return out << "SnapByClamshellTabletTransition";
    case WindowSnapActionSource::kSnapByDeskOrSessionChange:
      return out << "SnapByDeskOrSessionChange";
    case WindowSnapActionSource::kSnapGroupWindowUpdate:
      return out << "SnapGroupWindowUpdate";
    case WindowSnapActionSource::kTest:
      return out << "Test";
    case WindowSnapActionSource::kLacrosSnapButtonOrWindowLayoutMenu:
      return out << "LacrosSnapButtonOrWindowLayoutMenu";
    case WindowSnapActionSource::kSnapBySwapWindowsInSnapGroup:
      return out << "SnapBySwapWindowsInSnapGroup";
  }
}

}  // namespace ash
