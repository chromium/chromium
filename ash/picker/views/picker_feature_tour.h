// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_FEATURE_TOUR_H_
#define ASH_PICKER_VIEWS_PICKER_FEATURE_TOUR_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/unique_widget_ptr.h"

class PrefRegistrySimple;
class PrefService;

namespace views {
class Widget;
class Button;
}

namespace ash {

class ASH_EXPORT PickerFeatureTour {
 public:
  PickerFeatureTour();
  PickerFeatureTour(const PickerFeatureTour&) = delete;
  PickerFeatureTour& operator=(const PickerFeatureTour&) = delete;
  ~PickerFeatureTour();

  // Registers Picker feature tour prefs to the provided `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Disables the feature tour for tests.
  static void DisableFeatureTourForTesting();

  // Shows the feature tour dialog if the tour has not been shown before.
  // `completion_callback` is called when the user has completed the feature
  // tour.
  // Returns whether the feature tour dialog was shown or not.
  bool MaybeShowForFirstUse(PrefService* prefs,
                            base::RepeatingClosure completion_callback);

  views::Widget* widget_for_testing();

  // Returns the button to complete the tour.
  views::Button* complete_button_for_testing();

 private:
  views::UniqueWidgetPtr widget_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_FEATURE_TOUR_H_
