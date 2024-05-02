// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_ANCHORED_NUDGE_MANAGER_IMPL_H_
#define ASH_SYSTEM_TOAST_ANCHORED_NUDGE_MANAGER_IMPL_H_

#include <map>
#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_education_controller.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/system/toast/anchored_nudge.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace ui {
class ImplicitAnimationObserver;
}  // namespace ui

namespace views {
class LabelButton;
class View;
}  // namespace views

namespace ash {

struct AnchoredNudgeData;
class ScopedNudgePause;

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
  std::unique_ptr<ScopedNudgePause> CreateScopedPause() override;
  // TODO(b/296948349): Replace this with a new `GetNudge(id)` function as this
  // does not accurately reflect is a nudge is shown or not.
  bool IsNudgeShown(const std::string& id) override;

  // Removes all cached objects (e.g. observers, timers) related to a nudge when
  // its widget is destroying.
  void HandleNudgeWidgetDestroying(const std::string& id);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  const std::u16string& GetNudgeBodyTextForTest(const std::string& id);
  views::View* GetNudgeAnchorViewForTest(const std::string& id);
  views::LabelButton* GetNudgePrimaryButtonForTest(const std::string& id);
  views::LabelButton* GetNudgeSecondaryButtonForTest(const std::string& id);
  AnchoredNudge* GetShownNudgeForTest(const std::string& id);
  NudgeCatalogName GetNudgeCatalogNameForTest(const std::string& id);

  // TODO(b/297619385): Move constants to a new constants file.
  // Nudges with a body text that has at least this number of characters will
  // update its default duration to medium length.
  static constexpr int kLongBodyTextLength = 60;

  // Default duration that is used for nudges that expire.
  static constexpr base::TimeDelta kNudgeDefaultDuration = base::Seconds(6);

  // Duration used for nudges with a button or a body text that has
  // `kLongBodyTextLength` or more characters.
  static constexpr base::TimeDelta kNudgeMediumDuration = base::Seconds(10);

  // Duration used for nudges that are meant to persist until the user interacts
  // with them.
  static constexpr base::TimeDelta kNudgeLongDuration = base::Minutes(30);

  // If `shown_nudges_` contains `nudge_id`, returns the associated nudge.
  // Otherwise, returns nullptr.
  AnchoredNudge* GetNudgeIfShown(const std::string& nudge_id) const;

  // Resets the registry map that records the time a nudge was last shown.
  void ResetNudgeRegistryForTesting();

 private:
  friend class AnchoredNudgeManagerImplTest;
  class AnchorViewObserver;
  class AnchorViewWidgetObserver;
  class NudgeWidgetObserver;
  class PausableTimer;

  // Returns the registry which keeps track of when a nudge was last shown.
  static std::vector<std::pair<NudgeCatalogName, base::TimeTicks>>&
  GetNudgeRegistry();

  // Records the nudge `ShownCount` metric, and stores the time the nudge was
  // shown in the nudge registry.
  void RecordNudgeShown(NudgeCatalogName catalog_name);

  // Records button pressed metrics.
  void RecordButtonPressed(NudgeCatalogName catalog_name,
                           bool is_primary_button);

  // Closes all `shown_nudges_` immediately. Used for shutdown, when a scoped
  // nudge pause is activated, or when the session state changes.
  void CloseAllNudges();

  // Pauses or resumes the dismiss timer corresponding to `nudge_id`.
  // Called when:
  // 1. A nudge's mouse hover state changes. OR
  // 2. A nudge's child focus state changes.
  void PauseOrResumeDismissTimer(const std::string& nudge_id, bool pause);

  // Chains the provided `callback` to a `Cancel()` call to dismiss a nudge with
  // `id`, and returns this chained callback. If the provided `callback` is
  // empty, only a `Cancel()` callback will be returned.
  base::RepeatingClosure ChainCancelCallback(base::RepeatingClosure callback,
                                             NudgeCatalogName catalog_name,
                                             const std::string& id,
                                             bool is_primary_button);

  // AnchoredNudgeManager:
  void Pause() override;
  void Resume() override;

  // Maps an `AnchoredNudge` `id` to pointer to the nudge with that id.
  // Used to cache and keep track of nudges that are currently displayed, so
  // they can be dismissed or their contents updated.
  std::map<std::string, raw_ptr<AnchoredNudge>> shown_nudges_;

  // Maps an `AnchoredNudge` `id` to an observation of that nudge's
  // `anchor_view`, which is used to close the nudge whenever its anchor view is
  // deleting or hiding.
  std::map<std::string, std::unique_ptr<AnchorViewObserver>>
      anchor_view_observers_;

  // Maps an `AnchoredNudge` `id` to an observation of that nudge's
  // `anchor_view` widget, which is used to close the nudge whenever its anchor
  // view widget is deleting or hiding.
  std::map<std::string, std::unique_ptr<AnchorViewWidgetObserver>>
      anchor_view_widget_observers_;

  // Maps an `AnchoredNudge` `id` to an observation of that nudge's widget,
  // which is used to clean up the cached objects related to that nudge when its
  // widget is destroying.
  std::map<std::string, std::unique_ptr<NudgeWidgetObserver>>
      nudge_widget_observers_;

  // Maps an `AnchoredNudge` `id` to an observation of the nudge's hide
  // animation. Used to destroy the nudge widget on animation completed.
  std::map<std::string, std::unique_ptr<ui::ImplicitAnimationObserver>>
      hide_animation_observers_;

  // Maps an `AnchoredNudge` `id` to a timer that's used to dismiss the nudge
  // after its duration has passed. Hovering over the nudge pauses the timer.
  std::map<std::string, PausableTimer> dismiss_timers_;

  // Keeps track of the number of `ScopedNudgePause`.
  int pause_counter_ = 0;

  base::WeakPtrFactory<AnchoredNudgeManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_ANCHORED_NUDGE_MANAGER_IMPL_H_
