// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_READ_WRITE_CARD_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_READ_WRITE_CARD_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/gfx/geometry/rect.h"

class Profile;
class RenderViewContextMenuProxy;

namespace chromeos {
class ReadWriteCardController;
}  // namespace chromeos

// A class that observes context menu actions and notify all the associated
// `ReadWriteCardController` about the events.
class ReadWriteCardObserver : public RenderViewContextMenuObserver {
 public:
  ReadWriteCardObserver(const ReadWriteCardObserver&) = delete;
  ReadWriteCardObserver& operator=(const ReadWriteCardObserver&) = delete;

  ReadWriteCardObserver(RenderViewContextMenuProxy* proxy, Profile* profile);
  ~ReadWriteCardObserver() override;

  // RenderViewContextMenuObserver implementation.
  void CommandWillBeExecuted(int command_id) override;
  void OnContextMenuShown(const content::ContextMenuParams& params,
                          const gfx::Rect& bounds_in_screen) override;
  void OnContextMenuViewBoundsChanged(
      const gfx::Rect& bounds_in_screen) override;
  void OnMenuClosed() override;

 private:
  friend class ReadWriteCardObserverTest;

  void OnTextSurroundingSelectionAvailable(
      const std::u16string& selected_text,
      const std::u16string& surrounding_text,
      uint32_t start_offset,
      uint32_t end_offset);

  void OnFetchControllers(
      const content::ContextMenuParams& params,
      std::vector<base::WeakPtr<chromeos::ReadWriteCardController>>
          controllers);

  // The interface to add a context-menu item and update it.
  raw_ptr<RenderViewContextMenuProxy, DanglingUntriaged> proxy_;

  const raw_ptr<Profile> profile_;

  gfx::Rect bounds_in_screen_;

  // Whether commands other than quick answers is executed.
  bool is_other_command_executed_ = false;

  std::vector<raw_ptr<chromeos::ReadWriteCardController>>
      read_write_card_controllers_;

  base::WeakPtrFactory<ReadWriteCardObserver> weak_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_READ_WRITE_CARD_OBSERVER_H_
