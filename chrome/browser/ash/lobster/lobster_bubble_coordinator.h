// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_BUBBLE_COORDINATOR_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_BUBBLE_COORDINATOR_H_

#include <optional>
#include <string_view>

#include "ui/gfx/geometry/rect.h"

class WebUIContentsWrapper;
class Profile;

namespace ash {

// Class used to manage the state of Lobster WebUI bubble contents.
class LobsterBubbleCoordinator {
 public:
  LobsterBubbleCoordinator();
  LobsterBubbleCoordinator(const LobsterBubbleCoordinator&) = delete;
  LobsterBubbleCoordinator& operator=(const LobsterBubbleCoordinator&) = delete;
  ~LobsterBubbleCoordinator();

  void LoadUI(Profile* profile, std::optional<std::string_view> query);
  void ShowUI();
  void CloseUI();

 private:
  bool IsShowingUI() const;

  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_BUBBLE_COORDINATOR_H_
