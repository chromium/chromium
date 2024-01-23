// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_ASH_H_

#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// Implementation of `MahiManager`.
class MahiManagerAsh : public chromeos::MahiManager {
 public:
  MahiManagerAsh();

  MahiManagerAsh(const MahiManagerAsh&) = delete;
  MahiManagerAsh& operator=(const MahiManagerAsh&) = delete;

  ~MahiManagerAsh() override;

  // chromeos::MahiManager:
  void OpenMahiPanel(int64_t display_id) override;
  void GetSummary(MahiSummaryCallback callback) override;

 private:
  friend class MahiManagerAshTest;

  // The widget contains the Mahi main panel.
  views::UniqueWidgetPtr mahi_panel_widget_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_ASH_H_
