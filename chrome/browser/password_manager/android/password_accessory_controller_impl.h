// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace password_manager {
class ContentPasswordManagerDriver;
}  // namespace password_manager

class ManualFillingController;
class AllPasswordsBottomSheetController;

// Use either PasswordAccessoryController::GetOrCreate or
// PasswordAccessoryController::GetIfExisting to obtain instances of this class.
// This class exists for every tab and should never store state based on the
// contents of one of its frames. This can cause cross-origin hazards.
class PasswordAccessoryControllerImpl
    : public PasswordAccessoryController,
      public content::WebContentsUserData<PasswordAccessoryControllerImpl> {
 public:
  ~PasswordAccessoryControllerImpl() override;

  // AccessoryController:
  void OnFillingTriggered(const autofill::UserInfo::Field& selection) override;
  void OnOptionSelected(autofill::AccessoryAction selected_action) override;
  void OnToggleChanged(autofill::AccessoryAction toggled_action,
                       bool enabled) override;

  // PasswordAccessoryController:
  void RefreshSuggestionsForField(
      autofill::mojom::FocusedFieldType focused_field_type,
      bool is_manual_generation_available) override;
  void OnGenerationRequested(
      autofill::password_generation::PasswordGenerationType type) override;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Upon creation, a |credential_cache| is required
  // that will be queried for credentials.
  static void CreateForWebContents(
      content::WebContents* web_contents,
      password_manager::CredentialCache* credential_cache);

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows inject a manual filling
  // controller and a |PasswordManagerClient|.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      password_manager::CredentialCache* credential_cache,
      base::WeakPtr<ManualFillingController> mf_controller,
      password_manager::PasswordManagerClient* password_client);

  // True if the focus event was sent for the current focused frame or if it is
  // a blur event and no frame is focused. This check avoids reacting to
  // obsolete events that arrived in an unexpected order.
  // TODO(crbug.com/968162): Introduce the concept of active frame to the
  // accessory controller and move this check in the controller.
  static bool ShouldAcceptFocusEvent(
      content::WebContents* web_contents,
      password_manager::ContentPasswordManagerDriver* driver,
      autofill::mojom::FocusedFieldType focused_field_type);

 private:
  friend class content::WebContentsUserData<PasswordAccessoryControllerImpl>;

  // This constructor can also be used by |CreateForWebContentsForTesting|
  // to inject a fake |ManualFillingController| and a fake
  // |PasswordManagerClient|.
  PasswordAccessoryControllerImpl(
      content::WebContents* web_contents,
      password_manager::CredentialCache* credential_cache,
      base::WeakPtr<ManualFillingController> mf_controller,
      password_manager::PasswordManagerClient* password_client);

  // Enables or disables saving for the focused origin. This involves removing
  // or adding blacklisted entry in the |PasswordStore|.
  void ChangeCurrentOriginSavePasswordsStatus(bool enabled);

  // Returns true if |suggestion| matches a credential for |origin|.
  bool AppearsInSuggestions(const base::string16& suggestion,
                            bool is_password,
                            const url::Origin& origin) const;

  // Returns true if `field_type` and `origin` of a focused field allow to show
  // the option toggle to recover from a "never save" state.
  bool ShouldShowRecoveryToggle(autofill::mojom::FocusedFieldType field_type,
                                const url::Origin& origin) const;

  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization allows injecting mocks for tests.
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  // Instructs |AllPasswordsBottomSheetController| to show all passwords.
  void ShowAllPasswords();

  url::Origin GetFocusedFrameOrigin() const;

  // Called From |AllPasswordsBottomSheetController| when
  // the Bottom Sheet view is destroyed.
  void AllPasswordsSheetDismissed();

  // ------------------------------------------------------------------------
  // Members - Make sure to NEVER store state related to a single frame here!
  // ------------------------------------------------------------------------

  // The tab for which this class is scoped.
  content::WebContents* web_contents_ = nullptr;

  // Keeps track of credentials which are stored for all origins in this tab.
  password_manager::CredentialCache* credential_cache_ = nullptr;

  // The password accessory controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> mf_controller_;

  // The password manager client is used to update the save passwords status
  // for the currently focused origin.
  password_manager::PasswordManagerClient* password_client_ = nullptr;

  // Controller for the all passwords bottom sheet. Created on demand during the
  // first call to |ShowAllPasswords()|.
  std::unique_ptr<AllPasswordsBottomSheetController>
      all_passords_bottom_sheet_controller_;

  // Records the last focused field type that `RefreshSuggestionsForField()` was
  // called with.
  autofill::mojom::FocusedFieldType last_focused_field_type_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryControllerImpl);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
