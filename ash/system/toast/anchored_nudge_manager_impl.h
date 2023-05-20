// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_ANCHORED_NUDGE_MANAGER_IMPL_H_
#define ASH_SYSTEM_TOAST_ANCHORED_NUDGE_MANAGER_IMPL_H_

#include <map>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/system/toast/anchored_nudge.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace ash {

struct AnchoredNudgeData;

// Class managing anchored nudge requests.
class ASH_EXPORT AnchoredNudgeManagerImpl : public AnchoredNudgeManager,
                                            public AnchoredNudge::Delegate {
 public:
  AnchoredNudgeManagerImpl();
  AnchoredNudgeManagerImpl(const AnchoredNudgeManagerImpl&) = delete;
  AnchoredNudgeManagerImpl& operator=(const AnchoredNudgeManagerImpl&) = delete;
  ~AnchoredNudgeManagerImpl() override;

  // AnchoredNudgeManager:
  void Show(const AnchoredNudgeData& nudge_data) override;
  void Cancel(const std::string& id) override;

  // Closes all `shown_nudges_`.
  void CloseAllNudges();

  // Removes all cached objects (e.g. observers, timers) related to a nudge when
  // its widget is destroying.
  void HandleNudgeWidgetDestroying(const std::string& id);

  // AnchoredNudge::Delegate:
  void OnNudgeHoverStateChanged(const std::string& id,
                                bool is_hovering) override;

  bool IsNudgeShown(const std::string& id);
  const std::u16string& GetNudgeText(const std::string& id);
  views::View* GetNudgeAnchorView(const std::string& id);

  // Default nudge duration that is used for nudges that expire.
  static constexpr base::TimeDelta kAnchoredNudgeDuration = base::Seconds(6);

 private:
  friend class AnchoredNudgeManagerImplTest;
  class AnchorViewObserver;
  class NudgeWidgetObserver;

  // Manage the dismiss timer for the nudge with given `id`.
  void StartDismissTimer(const std::string& id);
  void StopDismissTimer(const std::string& id);

  // Maps an `AnchoredNudge` `id` to pointer to the nudge with that id.
  // Used to cache and keep track of nudges that are currently displayed, so
  // they can be dismissed or their contents updated.
  std::map<std::string, raw_ptr<AnchoredNudge>> shown_nudges_;

  // Maps an `AnchoredNudge` `id` to an observation of that nudge's
  // `anchor_view`, which is used to close the nudge whenever its anchor view is
  // deleting or hiding.
  std::map<std::string, std::unique_ptr<AnchorViewObserver>>
      anchor_view_observers_;

  // Maps an `AnchoredNudge` `id` to an observation of that nudge's widget,
  // which is used to clean up the cached objects related to that nudge when its
  // widget is destroying.
  std::map<std::string, std::unique_ptr<NudgeWidgetObserver>>
      nudge_widget_observers_;

  // Maps an `AnchoredNudge` `id` to a timer that's used to dismiss the nudge
  // after `kAnchoredNudgeDuration` has passed.
  std::map<std::string, base::OneShotTimer> dismiss_timers_;

  base::WeakPtrFactory<AnchoredNudgeManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_ANCHORED_NUDGE_MANAGER_IMPL_H_
