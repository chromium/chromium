// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_DRIVE_PICKER_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_DRIVE_PICKER_MANAGER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/host/glic_drive_picker_account_util.h"
#include "chrome/browser/glic/host/glic_drive_picker_types.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "google_apis/gaia/gaia_id.h"
#include "mojo/public/cpp/bindings/receiver.h"

class BrowserWindowInterface;
class DrivePickerHostController;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace views {
class Widget;
}  // namespace views

namespace glic {

class GlicEnabling;

// How a `GlicDrivePickerManager::ShowDrivePicker()` request ended.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicDrivePickerOutcome)
enum class GlicDrivePickerOutcome {
  // The user picked files and at least one was returned to the web client.
  kFilesPicked = 0,
  // The user cancelled from inside the picker without selecting files.
  // Pressing Escape closes the dialog widget directly and records
  // `kDialogClosed` instead.
  kCancelledByUser = 1,
  // The requesting Glic panel closed or the manager was destroyed.
  kCancelledByOwner = 2,
  // The picker reported an error that it could not classify.
  kUnknownError = 3,
  // The primary account is not signed in on the web (cookie jar).
  kWebSessionSignedOut = 4,
  // Chrome has no primary account, or its refresh token is missing or invalid.
  kChromeSignInRequired = 5,
  // The web client's account is not Chrome's primary account.
  kAccountMismatch = 6,
  // Another Drive picker is already open for this profile.
  kAlreadyActive = 7,
  // The Glic panel is hidden, or there is no window to anchor the picker to.
  kNoAnchorWindow = 8,
  // File upload is disabled by policy, or Glic is not enabled and consented.
  kDisabledByPolicy = 9,
  // The Drive picker is not supported for this profile. Not expected in
  // practice: `GlicWebClientHandler` rejects such calls before they reach the
  // manager.
  kUnsupported = 10,
  // The OAuth token for the picker could not be fetched.
  kTokenFetchFailed = 11,
  // The Google Picker API failed to load.
  kApiLoadFailed = 12,
  // The picker went away before returning a result.
  kPickerDisconnected = 13,
  // The picker page rejected the selection: the response was malformed.
  kInvalidResponse = 14,
  // The picker page rejected the selection: a file had an invalid ID.
  kInvalidFileId = 15,
  // The picker page rejected the selection: file metadata was missing or
  // invalid.
  kInvalidFileMetadata = 16,
  // The picker page rejected the selection: a file type is not supported.
  kUnsupportedFileType = 17,
  // The picker page rejected the selection: a file had an invalid size.
  kInvalidFileSize = 18,
  // The browser rejected the selection: it had too many files, or none of
  // them passed the browser-side sanitizer.
  kSelectionRejectedByBrowser = 19,
  // The picker dialog could not be created.
  kDialogCreationFailed = 20,
  // The picker dialog closed without the picker reporting a result (for
  // example, because the user pressed Escape or the window it was anchored to
  // closed).
  kDialogClosed = 21,
  kMaxValue = kDialogClosed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicDrivePickerOutcome)

// Manages Chrome's modal Google Drive picker (`chrome://drive-picker-host`)
// lifecycle for a Glic profile. Owned by `GlicKeyedService` so at most one
// Drive picker dialog is active per profile across all Glic instances (e.g.
// attached side panels and detached floating windows).
class GlicDrivePickerManager
    : public drive_picker_host::mojom::DrivePickerResultHandler {
 public:
  // Resolves with the sanitized selections (an empty vector indicates the user
  // cancelled or the owning Glic panel closed), or a `GlicDrivePickerError`.
  using ShowCallback = base::OnceCallback<void(
      base::expected<std::vector<GlicDriveFileSelection>,
                     GlicDrivePickerError>)>;

  using ControllerFactory =
      base::RepeatingCallback<std::unique_ptr<DrivePickerHostController>(
          Profile* profile,
          BrowserWindowInterface* browser,
          views::Widget* anchor_widget)>;

  // Maximum number of files accepted from a single Drive picker selection
  // response (matches the picker's `setMaxItems(10)` cap).
  static constexpr size_t kMaxSelections = 10;

  GlicDrivePickerManager(Profile* profile,
                         signin::IdentityManager* identity_manager,
                         GlicEnabling* enabling);
  GlicDrivePickerManager(const GlicDrivePickerManager&) = delete;
  GlicDrivePickerManager& operator=(const GlicDrivePickerManager&) = delete;
  ~GlicDrivePickerManager() override;

  // Validates eligibility, policy, panel visibility, concurrency, and account
  // readiness, then opens the Drive picker anchored to the window hosting
  // `webui_contents` (or the most recently active eligible browser window).
  // `owner_token` identifies the requesting web client handler so
  // `CancelForOwner()` only cancels sessions started by that handler.
  void ShowDrivePicker(const base::UnguessableToken& owner_token,
                       const GaiaId& expected_gaia,
                       bool is_panel_showing,
                       content::WebContents* webui_contents,
                       ShowCallback callback);

  // Cancels the active picker (resolving its callback with an empty selection
  // list) if it was opened by `owner_token`.
  void CancelForOwner(const base::UnguessableToken& owner_token);

  // Selects the best eligible browser window for `profile` to anchor the Drive
  // picker to: prefers the visible attachable browser window matching
  // `anchor_widget` (the attached side-panel case), otherwise falls back to the
  // most recently active visible attachable browser window (the detached floaty
  // case), or nullptr if none exists.
  static BrowserWindowInterface* FindAnchorBrowser(
      Profile* profile,
      views::Widget* anchor_widget);

  // Variant of `FindAnchorBrowser` accepting a custom visibility predicate for
  // unit tests that use `MockBrowserWindowInterface` without a full
  // `BrowserWindow`.
  static BrowserWindowInterface* FindAnchorBrowserForTesting(
      Profile* profile,
      views::Widget* anchor_widget,
      base::FunctionRef<bool(BrowserWindowInterface*)> is_browser_visible);

  bool IsPickerActiveForTesting() const { return controller_ != nullptr; }
  void SetControllerFactoryForTesting(ControllerFactory factory);

 private:
  // drive_picker_host::mojom::DrivePickerResultHandler:
  void OnSelection(
      std::vector<drive_picker_host::mojom::DriveFilePtr> files) override;
  void OnCancel() override;
  void OnError(drive_picker_host::mojom::DrivePickerError error) override;

  std::optional<GlicDrivePickerError> ValidateRequest(
      const GaiaId& expected_gaia,
      bool is_panel_showing) const;
  std::optional<GlicDrivePickerError> CheckActivePolicyAndEnabling() const;
  void LaunchPicker(const base::UnguessableToken& owner_token,
                    const GaiaId& expected_gaia,
                    content::WebContents* webui_contents,
                    ShowCallback callback);
  void OnPickerDisconnected();
  void OnPickerClosed();
  void OnAccountInvalidated(DrivePickerAccountError reason);
  void OnPolicyOrEnablingChanged();
  // Resolves the active request with `result` and records `outcome` in
  // metrics. `outcome` is passed separately because it is finer-grained than
  // `result`: it differentiates user cancels from owner cancels, and splits the
  // causes that the web client only sees as `kLoadFailed`.
  void FinishRequest(base::expected<std::vector<GlicDriveFileSelection>,
                                    GlicDrivePickerError> result,
                     GlicDrivePickerOutcome outcome);
  // Fails the active request with `error`, for errors that need no
  // finer-grained outcome.
  void FinishRequestWithError(GlicDrivePickerError error);
  void Cleanup();

  raw_ptr<Profile> profile_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  PrefChangeRegistrar pref_change_registrar_;
  base::CallbackListSubscription allowed_changed_subscription_;
  base::CallbackListSubscription consent_changed_subscription_;

  ControllerFactory controller_factory_;
  base::UnguessableToken active_owner_token_;
  GaiaId active_expected_gaia_;
  ShowCallback callback_;
  mojo::Receiver<drive_picker_host::mojom::DrivePickerResultHandler>
      result_handler_receiver_{this};
  std::unique_ptr<DrivePickerAccountWatcher> account_watcher_;
  std::unique_ptr<DrivePickerHostController> controller_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_DRIVE_PICKER_MANAGER_H_
