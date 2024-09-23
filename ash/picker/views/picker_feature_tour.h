// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_FEATURE_TOUR_H_
#define ASH_PICKER_VIEWS_PICKER_FEATURE_TOUR_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/wm/public/activation_change_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace aura {
class Window;
}

namespace views {
class Widget;
class Button;
}

namespace wm {
class ActivationClient;
}

namespace ash {

class ASH_EXPORT PickerFeatureTour : public wm::ActivationChangeObserver {
 public:
  enum class EditorStatus {
    kEligible,
    kNotEligible,
  };

  PickerFeatureTour();
  PickerFeatureTour(const PickerFeatureTour&) = delete;
  PickerFeatureTour& operator=(const PickerFeatureTour&) = delete;
  ~PickerFeatureTour() override;

  // Registers Picker feature tour prefs to the provided `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Disables the feature tour for tests.
  static void DisableFeatureTourForTesting();

  // Shows the feature tour dialog if the tour has not been shown before.
  // `learn_more_callback` is called when the user has asked for more
  // information. `completion_callback` is called when the user has completed
  // the feature tour. Returns whether the feature tour dialog was shown or not.
  // Both callbacks are guaranteed to be shown after the originally
  // focused/activated (possibly-null) window regains focus/activation.
  bool MaybeShowForFirstUse(PrefService* prefs,
                            EditorStatus editor_status,
                            base::RepeatingClosure learn_more_callback,
                            base::RepeatingClosure completion_callback);

  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  views::Widget* widget_for_testing();

  // Returns the button to learn more.
  const views::Button* learn_more_button_for_testing() const;
  // Returns the button to complete the tour.
  const views::Button* complete_button_for_testing() const;
  // Returns the title of the feature tour.
  std::u16string_view GetTitleTextForTesting() const;
  // Returns the description of the feature tour.
  std::u16string_view GetDescriptionForTesting() const;

 private:
  void SetOnWindowDeactivatedCallback(base::OnceClosure callback);
  void RunOnWindowDeactivatedIfNeeded();

  views::UniqueWidgetPtr widget_;

  base::OnceClosure on_window_deactivated_callback_;
  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      obs_{this};

  base::WeakPtrFactory<PickerFeatureTour> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_FEATURE_TOUR_H_
