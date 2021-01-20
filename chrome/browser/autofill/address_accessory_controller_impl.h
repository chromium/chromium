// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/address_accessory_controller.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class ManualFillingController;

namespace autofill {
class AutofillProfile;
class PersonalDataManager;

// Use either AddressAccessoryController::GetOrCreate or
// AddressAccessoryController::GetIfExisting to obtain instances of this class.
// This class exists for every tab and should never store state based on the
// contents of one of its frames.
class AddressAccessoryControllerImpl
    : public AddressAccessoryController,
      public PersonalDataManagerObserver,
      public content::WebContentsUserData<AddressAccessoryControllerImpl> {
 public:
  ~AddressAccessoryControllerImpl() override;

  // AccessoryController:
  void OnFillingTriggered(const autofill::UserInfo::Field& selection) override;
  void OnOptionSelected(AccessoryAction selected_action) override;
  void OnToggleChanged(AccessoryAction toggled_action, bool enabled) override;

  // AddressAccessoryController:
  void RefreshSuggestions() override;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows inject a manual filling
  // controller.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller);

 private:
  friend class content::WebContentsUserData<AddressAccessoryControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit AddressAccessoryControllerImpl(content::WebContents* contents);

  std::vector<autofill::AutofillProfile*> GetProfiles();

  // Constructor that allows to inject a mock filling controller.
  AddressAccessoryControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller);

  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization allows injecting mocks for tests.
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // The password accessory controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> mf_controller_;

  // The data manager used to retrieve the profiles.
  autofill::PersonalDataManager* personal_data_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AddressAccessoryControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_
