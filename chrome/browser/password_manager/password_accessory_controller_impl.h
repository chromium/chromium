// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace password_manager {
class ContentPasswordManagerDriver;
}  // namespace password_manager

class ManualFillingController;

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
  // controller.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      password_manager::CredentialCache* credential_cache,
      base::WeakPtr<ManualFillingController> mf_controller);

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

  // Required for construction via |CreateForWebContents|:
  PasswordAccessoryControllerImpl(
      content::WebContents* contents,
      password_manager::CredentialCache* credential_cache);

  // Constructor that allows to inject a mock or fake view.
  PasswordAccessoryControllerImpl(
      content::WebContents* web_contents,
      password_manager::CredentialCache* credential_cache,
      base::WeakPtr<ManualFillingController> mf_controller);

  // Returns true if |suggestion| matches a credential for |origin|.
  bool AppearsInSuggestions(const base::string16& suggestion,
                            bool is_password,
                            const url::Origin& origin) const;

  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization allows injecting mocks for tests.
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  url::Origin GetFocusedFrameOrigin() const;

  // ------------------------------------------------------------------------
  // Members - Make sure to NEVER store state related to a single frame here!
  // ------------------------------------------------------------------------

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // Keeps track of credentials which are stored for all origins in this tab.
  password_manager::CredentialCache* credential_cache_;

  // The password accessory controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> mf_controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryControllerImpl);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
