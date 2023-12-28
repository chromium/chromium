// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_TEST_API_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_TEST_API_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
}  // namespace base

namespace views {
class ImageView;
class View;
}  // namespace views

namespace ash {

class HoldingSpaceTray;

// Utility class to facilitate easier testing of holding space. This API mainly
// exists to workaround //ash dependency restrictions in browser tests.
class ASH_EXPORT HoldingSpaceTestApi {
 public:
  HoldingSpaceTestApi();
  HoldingSpaceTestApi(const HoldingSpaceTestApi&) = delete;
  HoldingSpaceTestApi& operator=(const HoldingSpaceTestApi&) = delete;
  ~HoldingSpaceTestApi();

  // Returns the root window that newly created windows should be added to.
  static aura::Window* GetRootWindowForNewWindows();

  // Shows holding space UI. This is a no-op if it's already showing.
  void Show();

  // Closes holding space UI. This is a no-op if it's already closed.
  void Close();

  // Returns true if holding space UI is showing, false otherwise.
  bool IsShowing();

  // Returns true if the holding space tray is showing in the shelf, false
  // otherwise.
  bool IsShowingInShelf();

  // Returns the item file path associated with the given `item_view`.
  const base::FilePath& GetHoldingSpaceItemFilePath(
      const views::View* item_view) const;

  // Returns the item ID associated with the given `item_view`.
  const std::string& GetHoldingSpaceItemId(const views::View* item_view) const;

  // Returns the holding space item view within `item_views` associated with the
  // specified `item_id`. If no associated holding space item view exists,
  // `nullptr` is returned.
  views::View* GetHoldingSpaceItemView(
      const std::vector<views::View*>& item_views,
      const std::string& item_id);

  // Returns all holding space item views regardless of the section in which
  // they reside. Views are returned in top-to-bottom, left-to-right order (or
  // mirrored for RTL).
  std::vector<views::View*> GetHoldingSpaceItemViews();

  // Returns the container of the suggestions section in holding space UI.
  views::View* GetSuggestionsSectionContainer();

  // Returns the header of the suggestions section in holding space UI.
  views::View* GetSuggestionsSectionHeader();

  // Returns the header of the downloads section in holding space UI.
  views::View* GetDownloadsSectionHeader();

  // Returns the collection of download chips in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetDownloadChips();

  // Returns the collection of pinned file chips in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetPinnedFileChips();

  // Returns the collection of screen capture views in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetScreenCaptureViews();

  // Returns the collection of suggestion chips in holding space UI.
  // If holding space UI is not visible, an empty collection is returned.
  std::vector<views::View*> GetSuggestionChips();

  // Returns the holding space tray in the shelf.
  views::View* GetTray();

  // Returns the view drawn on top of the holding space tray to indicate that
  // it is a drop target capable of handling the current drag payload.
  views::View* GetTrayDropTargetOverlay();

  // Returns the holding space tray icon view for the default, non content
  // forward  icon.
  views::ImageView* GetDefaultTrayIcon();

  // Returns the holding space tray icon view for the content forward icon,
  // which displays previews of most recent items added to holding space.
  views::View* GetPreviewsTrayIcon();

  // Returns the view of the icon used for toggling the suggestions section's
  // expanded state.
  views::ImageView* GetSuggestionsSectionChevronIcon();

  // Returns the top-level bubble.
  views::View* GetBubble();

  // Returns the pinned files bubble.
  views::View* GetPinnedFilesBubble();

  // Returns whether the pinned files bubble is shown.
  bool PinnedFilesBubbleShown() const;

  // Returns the recent files bubble.
  views::View* GetRecentFilesBubble();

  // Returns whether the recent files bubble is shown.
  bool RecentFilesBubbleShown() const;

  // Returns whether the recent files placeholder is shown.
  bool RecentFilesPlaceholderShown() const;

 private:
  raw_ptr<HoldingSpaceTray, DanglingUntriaged> holding_space_tray_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_TEST_API_H_
