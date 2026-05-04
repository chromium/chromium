// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_PREFS_MERGE_CONFLICT_DIALOG_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_PREFS_MERGE_CONFLICT_DIALOG_H_

#include "ash/ash_export.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "ui/gfx/vector_icon_types.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class AccessibilityPrefsMergeConflictController;
class HoverHighlightView;

// This class is used to resolve conflicts between accessibility preferences
// set locally at OOBE/login screen at the time the account is created, and the
// respective values set remotely previously.
class ASH_EXPORT AccessibilityPrefsMergeConflictDialog
    : public SystemDialogDelegateView {
  METADATA_HEADER(AccessibilityPrefsMergeConflictDialog,
                  SystemDialogDelegateView)

 public:
  // Creates and shows the dialog at the center of the primary display.
  static std::unique_ptr<views::Widget> CreateAndShow(
      std::unique_ptr<AccessibilityPrefsMergeConflictController> controller,
      base::OnceCallback<void()> on_dismissed);

  AccessibilityPrefsMergeConflictDialog(
      const AccessibilityPrefsMergeConflictDialog&) = delete;
  AccessibilityPrefsMergeConflictDialog& operator=(
      const AccessibilityPrefsMergeConflictDialog&) = delete;
  ~AccessibilityPrefsMergeConflictDialog() override;

 private:
  AccessibilityPrefsMergeConflictDialog(
      std::unique_ptr<AccessibilityPrefsMergeConflictController> controller,
      base::OnceCallback<void()> on_dismissed);

  void BuildResolutionList(views::View* main_container);
  HoverHighlightView* AddScrollListToggleItem(views::View* container,
                                              const gfx::VectorIcon& icon,
                                              const std::u16string& text,
                                              std::string_view pref_name,
                                              bool checked,
                                              bool enterprise_managed = false);
  HoverHighlightView* AddScrollListItem(views::View* container,
                                        const gfx::VectorIcon& icon,
                                        const std::u16string& text);

  // WidgetDelegate:
  void WindowClosing() override;

  void OnResolvePrefsAccepted();
  void OnShowAccessibilitySettings();
  void OnPrefRowPressed(HoverHighlightView* item, std::string_view pref_name);

  std::unique_ptr<AccessibilityPrefsMergeConflictController> controller_;

  base::OnceCallback<void()> on_dismissed_;

  base::WeakPtrFactory<AccessibilityPrefsMergeConflictDialog> weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_PREFS_MERGE_CONFLICT_DIALOG_H_
