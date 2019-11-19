// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PARENT_ACCESS_WIDGET_H_
#define ASH_LOGIN_UI_PARENT_ACCESS_WIDGET_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"

class AccountId;

namespace views {
class Widget;
}

namespace ash {

class WindowDimmer;
class ParentAccessView;

enum class ParentAccessRequestReason;

// Widget to display the Parent Access View in a standalone container.
// This widget is modal and only one instance can be created at a time. It will
// be destroyed when dismissed.
class ASH_EXPORT ParentAccessWidget {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(ParentAccessWidget* widget);
    ~TestApi();

    ParentAccessView* parent_access_view();

    // Simulates that parent access code validation finished with the result
    // specified in |access_granted|, which dismisses the widget.
    void SimulateValidationFinished(bool access_granted);

   private:
    ParentAccessWidget* const parent_access_widget_;
  };

  // Callback for Parent Access Code validations. It is called when the widget
  // is about to be dismissed. |success| tells whether the validation was
  // successful.
  using OnExitCallback = base::RepeatingCallback<void(bool success)>;

  // Creates and shows the instance of ParentAccessWidget.
  // This widget is modal and only one instance can be created at a time. It
  // will be destroyed when dismissed.
  // When |account_id| is valid, the parent access code is validated using the
  // configuration for the provided account, when it is empty it tries to
  // validate the access code to any child signed in the device.
  // The |callback| is called when (a) the validation is successful or (b) the
  // back button is pressed.
  // |reason| contains information about why the parent access view is
  // necessary, it is used to modify the widget appearance by changing the title
  // and description strings and background color. The parent access widget is a
  // modal and already contains a dimmer, however when another modal is the
  // parent of the widget, the dimmer will be placed behind the two windows.
  // |extra_dimmer| will create an extra dimmer between the two.
  // |validation_time| is the time that will be used to validate the code, if
  // null the system's current time will be used.
  static void Show(const AccountId& account_id,
                   OnExitCallback callback,
                   ParentAccessRequestReason reason,
                   bool extra_dimmer,
                   base::Time validation_time);
  static void Show(const AccountId& account_id,
                   OnExitCallback callback,
                   ParentAccessRequestReason reason);

  // Returns the instance of ParentAccessWidget or nullptr if it does not exits.
  static ParentAccessWidget* Get();

  // Destroys the instance of ParentAccessWidget.
  void Destroy();

 private:
  ParentAccessWidget(const AccountId& account_id,
                     OnExitCallback callback,
                     ParentAccessRequestReason reason,
                     bool extra_dimmer,
                     base::Time validation_time);
  ~ParentAccessWidget();

  // Shows the |widget_| and |dimmer_| if applicable.
  void Show();

  // Closes the widget and forwards the result to the validation to |callback_|.
  void OnExit(bool success);

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<WindowDimmer> dimmer_;

  OnExitCallback callback_;

  base::WeakPtrFactory<ParentAccessWidget> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ParentAccessWidget);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PARENT_ACCESS_WIDGET_H_
