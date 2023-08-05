// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_ANCHORED_NUDGE_MANAGER_IMPL_H_
#define ASH_SYSTEM_TOAST_ANCHORED_NUDGE_MANAGER_IMPL_H_

#include <map>
#include <string>

#include "ash/ash_export.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/system/toast/anchored_nudge.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace views {
class LabelButton;
class View;
}  // namespace views

namespace ash {

struct AnchoredNudgeData;
class ScopedAnchoredNudgePause;

// Class managing anchored nudge requests.
class ASH_EXPORT AnchoredNudgeManagerImpl : public AnchoredNudgeManager,
                                            public SessionObserver {
 public:
  AnchoredNudgeManagerImpl();
  AnchoredNudgeManagerImpl(const AnchoredNudgeManagerImpl&) = delete;
  AnchoredNudgeManagerImpl& operator=(const AnchoredNudgeManagerImpl&) = delete;
  ~AnchoredNudgeManagerImpl() override;

  // AnchoredNudgeManager:
  void Show(AnchoredNudgeData& nudge_data) override;
  void Cancel(const std::string& id) override;
  void MaybeRecordNudgeAction(NudgeCatalogName catalog_name) override;
  std::unique_ptr<ScopedAnchoredNudgePause> CreateScopedPause() override;
  bool IsNudgeShown(const std::string& id) override;

  // Closes all `shown_nudges_`.
  void CloseAllNudges();

  // Removes all cached objects (e.g. observers, timers) related to a nudge when
  // its widget is destroying.
  void HandleNudgeWidgetDestroying(const std::string& id);

  // AnchoredNudge::Delegate:
  void OnNudgeHoverStateChanged(const std::string& nudge_id, bool is_hovering);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  const std::u16string& GetNudgeBodyTextForTest(const std::string& id);
  views::View* GetNudgeAnchorViewForTest(const std::string& id);
  views::LabelButton* GetNudgeFirstButtonForTest(const std::string& id);
  views::LabelButton* GetNudgeSecondButtonForTest(const std::string& id);
  AnchoredNudge* GetShownNudgeForTest(const std::string& id);

  // Default duration that is used for nudges that expire.
  static constexpr base::TimeDelta kNudgeDefaultDuration = base::Seconds(6);

  // Duration for nudges that are meant to persist until user has interacted
  // with them.
  static constexpr base::TimeDelta kNudgeLongDuration = base::Minutes(30);

  // Resets the registry map that records the time a nudge was last shown.
  void ResetNudgeRegistryForTesting();

  // Records button pressed metrics.
  void RecordButtonPressed(NudgeCatalogName catalog_name, bool first_button);

 private:
  friend class AnchoredNudgeManagerImplTest;
  class AnchorViewObserver;
  class NudgeWidgetObserver;
  class NudgeHoverObserver;
  class PausableTimer;

  // Returns the registry which keeps track of when a nudge was last shown.
  static std::vector<std::pair<NudgeCatalogName, base::TimeTicks>>&
  GetNudgeRegistry();

  // Records the nudge `ShownCount` metric, and stores the time the nudge was
  // shown in the nudge registry.
  void RecordNudgeShown(NudgeCatalogName catalog_name);

  // Chains the provided `callback` to a `Cancel()` call to dismiss a nudge with
  // `id`, and returns this chained callback. If the provided `callback` is
  // empty, only a `Cancel()` callback will be returned.
  base::RepeatingClosure ChainCancelCallback(base::RepeatingClosure callback,
                                             NudgeCatalogName catalog_name,
                                             const std::string& id,
                                             bool first_button);

  // AnchoredNudgeManager:
  void Pause() override;
  void Resume() override;

  // Maps an `AnchoredNudge` `id` to pointer to the nudge with that id.
  // Used to cache and keep track of nudges that are currently displayed, so
  // they can be dismissed or their contents updated.
  std::map<std::string, raw_ptr<AnchoredNudge>> shown_nudges_;

  std::map<std::string, std::unique_ptr<NudgeHoverObserver>>
      nudge_hover_observers_;

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
  // after its duration has passed. Hovering over the nudge pauses the timer.
  std::map<std::string, PausableTimer> dismiss_timers_;

  // Keeps track of the number of `ScopedAnchoredNudgePause`.
  int pause_counter_ = 0;

  base::WeakPtrFactory<AnchoredNudgeManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_ANCHORED_NUDGE_MANAGER_IMPL_H_
