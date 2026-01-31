// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_
#define CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_

#include "ash/system/magic_boost/magic_boost_constants.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// `MagicBoostController` is the central point to deal with the ChromeOS -
// Chrome browser communication. it is responsible for showing the disclaimer UI
// and connect with Orca services in ash.
class MagicBoostController {
 public:
  static MagicBoostController* Get();

  virtual void ShowDisclaimerUi(int64_t display_id,
                                magic_boost::TransitionAction action,
                                magic_boost::OptInFeatures opt_in_features) = 0;

  virtual void CloseDisclaimerUi() = 0;

 protected:
  MagicBoostController();
  MagicBoostController(const MagicBoostController&) = delete;
  MagicBoostController& operator=(const MagicBoostController&) = delete;
  virtual ~MagicBoostController();
};

class MagicBoostControllerImpl : public MagicBoostController {
 public:
  MagicBoostControllerImpl();
  ~MagicBoostControllerImpl() override;

  void ShowDisclaimerUi(int64_t display_id,
                        magic_boost::TransitionAction action,
                        magic_boost::OptInFeatures opt_in_features) override;

  void CloseDisclaimerUi() override;

  views::Widget* disclaimer_widget_for_test() {
    return disclaimer_widget_.get();
  }

 private:
  friend class MagicBoostControllerTest;

  // Called when the disclaimer view's accept button is clicked. `display_id`
  // indicates the display where the disclaimer view shows. `action` specifies
  // the action to take after the opt-in flow.
  void OnDisclaimerAcceptButtonPressed(int64_t display_id,
                                       magic_boost::TransitionAction action);

  // Called when the diclaimer view's declination button is clicked.
  void OnDisclaimerDeclineButtonPressed();
  void OnLinkPressed(const std::string& url);

  views::UniqueWidgetPtr disclaimer_widget_;
  magic_boost::OptInFeatures opt_in_features_;
  base::WeakPtrFactory<MagicBoostControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_
