// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_TOOL_H_
#define ASH_SYSTEM_PALETTE_PALETTE_TOOL_H_

#include "ash/ash_export.h"
#include "ash/system/palette/palette_ids.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/vector_icon_types.h"

namespace aura {
class Window;
}

namespace gfx {
struct VectorIcon;
}

namespace views {
class View;
}

namespace ash {

enum class PaletteGroup;
enum class PaletteToolId;
class PaletteToolManager;

// A PaletteTool is a generalized action inside of the palette menu in the
// shelf. Only one tool per group is active at any given time. When the tool is
// active, it should be showing some specializied UI. The tool is no longer
// active if it completes its action, if the user selects another tool with the
// same group, or if the user just cancels the action from the palette.
class ASH_EXPORT PaletteTool {
 public:
  class Delegate {
   public:
    // Enable or disable a specific tool.
    virtual void EnableTool(PaletteToolId tool_id) = 0;
    virtual void DisableTool(PaletteToolId tool_id) = 0;

    // Hide the entire palette. This should not change any tool state.
    virtual void HidePalette() = 0;

    // Hide the entire palette without showing a hide animation.
    virtual void HidePaletteImmediately() = 0;

    // Returns the root window.
    virtual aura::Window* GetWindow() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Adds all available PaletteTool instances to the tool_manager.
  static void RegisterToolInstances(PaletteToolManager* tool_manager);

  // |delegate| must outlive this tool instance.
  explicit PaletteTool(Delegate* delegate);

  PaletteTool(const PaletteTool&) = delete;
  PaletteTool& operator=(const PaletteTool&) = delete;

  virtual ~PaletteTool();

  // The group this tool belongs to. Only one tool per group can be active at
  // any given time.
  virtual PaletteGroup GetGroup() const = 0;

  // The unique identifier for this tool. This should be the only tool that ever
  // has this ID.
  virtual PaletteToolId GetToolId() const = 0;

  // Called when the user activates the tool. Only one tool per group can be
  // active at any given time.
  virtual void OnEnable();

  // Disable the tool, either because this tool called DisableSelf(), the
  // user cancelled the tool, or the user activated another tool within the
  // same group.
  virtual void OnDisable();

  // Create a view that will be used in the palette, or nullptr if this tool
  // should not be displayed. The view is owned by the caller. OnViewDestroyed
  // is called when the view has been deallocated by its owner.
  virtual views::View* CreateView() = 0;
  virtual void OnViewDestroyed() = 0;

  // Returns an icon to use in the tray if this tool is active. Only one tool
  // (per-group) should ever have an active icon at any given time. The icon
  // will be the same as that used in the palette tray on the left-most edge of
  // the tool i.e. CommonPaletteTool::GetPaletteIcon().
  // TODO(michelefan): Consider using the same function to return
  // icon for palette menu and palette tray at the status area.
  virtual const gfx::VectorIcon& GetActiveTrayIcon() const;

  void SetExternalDisplayForTest() { external_display_for_test_ = true; }

 protected:
  // Enables/disables the tool.
  bool enabled() const { return enabled_; }

  Delegate* delegate() { return delegate_; }

  bool external_display_for_test_ = false;

 private:
  bool enabled_ = false;

  // Unowned pointer to the delegate. The delegate should outlive this instance.
  raw_ptr<Delegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_TOOL_H_
