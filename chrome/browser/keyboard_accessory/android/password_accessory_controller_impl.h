// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/affiliated_plus_profiles_provider.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge.h"
#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_helper.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/native_widget_types.h"

class ManualFillingController;
class AllPasswordsBottomSheetController;
class Profile;

namespace plus_addresses {
class AllPlusAddressesBottomSheetController;
class PlusAddressService;
}  // namespace plus_addresses

// Use either PasswordAccessoryController::GetOrCreate or
// PasswordAccessoryController::GetIfExisting to obtain instances of this class.
// This class exists for every tab and should never store state based on the
// contents of one of its frames. This can cause cross-origin hazards.
class PasswordAccessoryControllerImpl
    : public PasswordAccessoryController,
      public AffiliatedPlusProfilesProvider::Observer,
      public content::WebContentsObserver,
      public content::WebContentsUserData<PasswordAccessoryControllerImpl> {
 public:
  using PasswordDriverSupplierForFocusedFrame =
      base::RepeatingCallback<password_manager::PasswordManagerDriver*(
          content::WebContents*)>;
  using ShowMigrationWarningCallback = base::RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>;

  PasswordAccessoryControllerImpl(const PasswordAccessoryControllerImpl&) =
      delete;
  PasswordAccessoryControllerImpl& operator=(
      const PasswordAccessoryControllerImpl&) = delete;

  ~PasswordAccessoryControllerImpl() override;

  // AccessoryController:
  void RegisterFillingSourceObserver(FillingSourceObserver observer) override;
  std::optional<autofill::AccessorySheetData> GetSheetData() const override;
  void OnFillingTriggered(
      autofill::FieldGlobalId focused_field_id,
      const autofill::AccessorySheetField& selection) override;
  void OnPasskeySelected(const std::vector<uint8_t>& passkey_id) override;
  void OnOptionSelected(autofill::AccessoryAction selected_action) override;
  void OnToggleChanged(autofill::AccessoryAction toggled_action,
                       bool enabled) override;

  // PasswordAccessoryController:
  void RegisterPlusProfilesProvider(
      base::WeakPtr<AffiliatedPlusProfilesProvider> provider) override;
  void RefreshSuggestionsForField(
      autofill::mojom::FocusedFieldType focused_field_type) override;
  void OnGenerationRequested(
      autofill::password_generation::PasswordGenerationType type) override;
  void UpdateCredManReentryUi(
      autofill::mojom::FocusedFieldType focused_field_type) override;
  base::WeakPtr<PasswordAccessoryController> AsWeakPtr() override;

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
      base::WeakPtr<ManualFillingController> manual_filling_controller,
      password_manager::PasswordManagerClient* password_client,
      PasswordDriverSupplierForFocusedFrame driver_supplier,
      ShowMigrationWarningCallback show_migration_warning_callback,
      std::unique_ptr<PasswordAccessLossWarningBridge>
          access_loss_warning_bridge);

  // Returns true if the current site attached to `web_contents_` has a SECURE
  // security level.
  bool IsSecureSite() const;

#if defined(UNIT_TEST)
  // Used for testing to set `security_level_for_testing_`.
  void SetSecurityLevelForTesting(
      security_state::SecurityLevel security_level) {
    security_level_for_testing_ = security_level;
  }
#endif
 protected:
  // This constructor can also be used by |CreateForWebContentsForTesting|
  // to inject a fake |ManualFillingController| and a fake
  // |PasswordManagerClient|.
  PasswordAccessoryControllerImpl(
      content::WebContents* web_contents,
      password_manager::CredentialCache* credential_cache,
      base::WeakPtr<ManualFillingController> manual_filling_controller,
      password_manager::PasswordManagerClient* password_client,
      PasswordDriverSupplierForFocusedFrame driver_supplier,
      ShowMigrationWarningCallback show_migration_warning_callback,
      std::unique_ptr<PasswordAccessLossWarningBridge>
          access_loss_warning_bridge);

 private:
  friend class content::WebContentsUserData<PasswordAccessoryControllerImpl>;

  // This struct is used to remember the meta information about the last focused
  // field.
  struct LastFocusedFieldInfo {
    LastFocusedFieldInfo(url::Origin focused_origin,
                         autofill::mojom::FocusedFieldType focused_field,
                         bool manual_generation_available);

    // Records the origin at the time of focusing the field to double-check that
    // the frame origin hasn't changed.
    url::Origin origin;

    // Records the last focused field type to infer whether the accessory is
    // available and whether passwords or usernames will be fillable.
    autofill::mojom::FocusedFieldType focused_field_type =
        autofill::mojom::FocusedFieldType::kUnknown;

    // If true, manual generation will be available for the focused field.
    bool is_manual_generation_available = false;
  };

  // WebContentsObserver:
  void WebContentsDestroyed() override;

  // Constructs a vector of available manual fallback actions subject to
  // enabled features and available user data.
  std::vector<autofill::FooterCommand> CreateManagePasswordsFooter() const;

  // Enables or disables saving for the focused origin. This involves removing
  // or adding blocklisted entry in the |PasswordStore|.
  void ChangeCurrentOriginSavePasswordsStatus(bool enabled);

  // Returns true if |suggestion| matches a credential for |origin|.
  bool AppearsInSuggestions(const std::u16string& suggestion,
                            bool is_password,
                            const url::Origin& origin) const;

  // Returns true if the `origin` of a focused field allows to show
  // the option toggle to recover from a "never save" state.
  bool ShouldShowRecoveryToggle(const url::Origin& origin) const;

  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization allows injecting mocks for tests.
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  // Instructs |AllPasswordsBottomSheetController| to show all passwords.
  void ShowAllPasswords();

  url::Origin GetFocusedFrameOrigin() const;

  // Returns true if authentication should be triggered before filling
  // |selection| in to the field.
  bool ShouldTriggerBiometricReauth(
      const autofill::AccessorySheetField& selection) const;

  // Called when the biometric authentication completes. If |auth_succeeded| is
  // true, |selection| will be passed on to be filled.
  void OnReauthCompleted(autofill::AccessorySheetField selection,
                         bool auth_succeeded);

  // Sends |selection| to the renderer to be filled, if it's a valid
  // entry for the origin of the frame that is currently focused.
  void FillSelection(const autofill::AccessorySheetField& selection);

  // Called From |AllPasswordsBottomSheetController| when
  // the Bottom Sheet view is destroyed.
  void AllPasswordsSheetDismissed();

  // Fills `plus_address` into the currently focused field. Called when the
  // manually triggered plus address creation bottom sheet is accepted by the
  // user.
  void OnPlusAddressCreated(const std::string& plus_address);

  // Triggers the filling `plus_address` into the currently focused field.
  void OnPlusAddressSelected(
      base::optional_ref<const std::string> plus_address);

  // Fetches suggestions and propagates them to the frontend.
  void RefreshSuggestions();

  // AffiliatedPlusProfilesProvider::Observer:
  void OnAffiliatedPlusProfilesFetched() override;

  content::WebContents& GetWebContents() const;

  // Keeps track of credentials which are stored for all origins in this tab.
  const raw_ptr<password_manager::CredentialCache> credential_cache_;

  // The password accessory controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> manual_filling_controller_;

  // The plus profiles provider that is used to generate the plus profiles
  // section for the frontend.
  base::WeakPtr<AffiliatedPlusProfilesProvider> plus_profiles_provider_;

  // The password manager client is used to update the save passwords status
  // for the currently focused origin.
  const raw_ptr<password_manager::PasswordManagerClient> password_client_;

  // The authenticator used to trigger a biometric re-auth before filling.
  // null, if there is no ongoing authentication.
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator_;

  // Information about the currently focused field. This is the only place
  // allowed to store frame-specific data. If a new field is focused or focus is
  // lost, this data needs to be reset to std::nullopt to make sure that data
  // related to a former frame isn't displayed incorrectly in a different one.
  std::optional<LastFocusedFieldInfo> last_focused_field_info_ = std::nullopt;

  // The observer to notify if available suggestions change.
  FillingSourceObserver source_observer_;

  // Callback that returns a |PasswordManagerDriver| corresponding to the
  // currently-focused frame of the passed-in |WebContents|.
  PasswordDriverSupplierForFocusedFrame driver_supplier_;

  // Controller for the all passwords bottom sheet. Created on demand during the
  // first call to |ShowAllPasswords()|.
  std::unique_ptr<AllPasswordsBottomSheetController>
      all_passords_bottom_sheet_controller_;

  // Helper for determining whether a bottom sheet showing passwords is useful.
  AllPasswordsBottomSheetHelper all_passwords_helper_{
      password_client_->GetProfilePasswordStore(),
      password_client_->GetAccountPasswordStore()};

  // Security level used for testing only.
  security_state::SecurityLevel security_level_for_testing_ =
      security_state::NONE;

  // Callback attempting to display the migration warning when invoked.
  // Used to facilitate injecting a mock bridge in tests.
  ShowMigrationWarningCallback show_migration_warning_callback_;

  // Bridge used for showing the password access loss warning sheet after
  // filling credentials.
  std::unique_ptr<PasswordAccessLossWarningBridge> access_loss_warning_bridge_;

  const raw_ptr<const plus_addresses::PlusAddressService> plus_address_service_;

  std::unique_ptr<plus_addresses::AllPlusAddressesBottomSheetController>
      all_plus_addresses_bottom_sheet_controller_;

  base::WeakPtrFactory<PasswordAccessoryControllerImpl> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
