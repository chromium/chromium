// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_BUBBLE_COORDINATOR_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_BUBBLE_COORDINATOR_H_

#include <optional>
#include <string_view>

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "base/scoped_observation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_observer.h"

class WebUIContentsWrapper;
class Profile;

namespace ash {

// Class used to manage the state of Lobster WebUI bubble contents.
class LobsterBubbleCoordinator : public views::WidgetObserver {
 public:
  LobsterBubbleCoordinator();
  LobsterBubbleCoordinator(const LobsterBubbleCoordinator&) = delete;
  LobsterBubbleCoordinator& operator=(const LobsterBubbleCoordinator&) = delete;
  ~LobsterBubbleCoordinator() override;

  void LoadUI(Profile* profile,
              std::optional<std::string_view> query,
              LobsterMode mode);
  void ShowUI();
  void CloseUI();

 private:
  bool IsShowingUI() const;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_BUBBLE_COORDINATOR_H_
