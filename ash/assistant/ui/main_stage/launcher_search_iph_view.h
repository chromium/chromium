// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_LAUNCHER_SEARCH_IPH_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_LAUNCHER_SEARCH_IPH_VIEW_H_

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_client.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
}  // namespace views

namespace ash {

class ChipView;
class PillButton;

class LauncherSearchIphView : public views::View {
  METADATA_HEADER(LauncherSearchIphView, views::View)

 public:
  // Delegate for handling actions of `LauncherSearchIphView`.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Run `query` as a launcher search. `query` is localized.
    virtual void RunLauncherSearchQuery(const std::u16string& query) = 0;
    // Opens Assistant page in the launcher.
    virtual void OpenAssistantPage() = 0;
  };

  // Event names live in a global namespace. Prefix with the feature name to
  // prevent unintentional name collisions.
  static constexpr char kIphEventNameChipClick[] =
      "IPH_LauncherSearchHelpUi_chip_click";
  static constexpr char kIphEventNameAssistantClick[] =
      "IPH_LauncherSearchHelpUi_assistant_click";

  enum ViewId {
    kSelf = 1,
    kAssistant,
    // Do not put a new id after `kChipStart`. Numbers after `kChipStart`
    // will be used for chips.
    kChipStart
  };

  enum class UiLocation {
    // In the Launcher search box.
    kSearchBox = 0,
    // In the `assistant_page` in the Launcher.
    kAssistantPage
  };

  static void SetChipTextForTesting(const std::u16string& text);

  LauncherSearchIphView(Delegate* delegate,
                        bool is_in_tablet_mode,
                        std::unique_ptr<ScopedIphSession> scoped_iph_session,
                        UiLocation location);
  ~LauncherSearchIphView() override;

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  void NotifyAssistantButtonPressedEvent();

  std::u16string GetTitleText() const;

  std::vector<raw_ptr<ChipView>> GetChipsForTesting();
  views::View* GetAssistantButtonForTesting();

 private:
  void RunLauncherSearchQuery(assistant::LauncherSearchIphQueryType query_type);

  void OpenAssistantPage();

  void CreateChips(views::BoxLayoutView* actions_container);

  void ShuffleChipsQuery();

  void SetChipsVisibility();

  raw_ptr<Delegate> delegate_ = nullptr;

  bool is_in_tablet_mode_ = false;

  std::unique_ptr<ScopedIphSession> scoped_iph_session_;

  UiLocation location_ = UiLocation::kSearchBox;

  std::vector<raw_ptr<ChipView>> chips_;
  raw_ptr<ash::PillButton> assistant_button_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;

  base::WeakPtrFactory<LauncherSearchIphView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_LAUNCHER_SEARCH_IPH_VIEW_H_
