// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_drive_picker_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_sanitizer.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

GlicDrivePickerError AccountErrorToPickerError(DrivePickerAccountError error) {
  switch (error) {
    case DrivePickerAccountError::kChromeSignInRequired:
      return GlicDrivePickerError::kChromeSignInRequired;
    case DrivePickerAccountError::kAccountMismatch:
      return GlicDrivePickerError::kAccountMismatch;
    case DrivePickerAccountError::kWebSessionSignedOut:
      return GlicDrivePickerError::kWebSessionSignedOut;
  }
}

GlicDrivePickerError DrivePickerErrorToReason(
    drive_picker_host::mojom::DrivePickerError error) {
  using drive_picker_host::mojom::DrivePickerError;
  switch (error) {
    case DrivePickerError::kAlreadyActive:
      return GlicDrivePickerError::kAlreadyActive;
    case DrivePickerError::kWindowNotFound:
      return GlicDrivePickerError::kNoAnchorWindow;
    case DrivePickerError::kTokenFetchFailure:
    case DrivePickerError::kApiLoadFailure:
    case DrivePickerError::kInvalidResponse:
    case DrivePickerError::kInvalidFileId:
    case DrivePickerError::kInvalidFileMetadata:
    case DrivePickerError::kUnsupportedFileType:
    case DrivePickerError::kInvalidFileSize:
    case DrivePickerError::kMojoDisconnected:
    case DrivePickerError::kWebUINotFound:
    case DrivePickerError::kViewNotFound:
      return GlicDrivePickerError::kLoadFailed;
    case DrivePickerError::kUnknown:
      return GlicDrivePickerError::kUnknown;
  }
}

GlicDrivePickerOutcome OutcomeForError(GlicDrivePickerError error) {
  switch (error) {
    case GlicDrivePickerError::kUnknown:
      return GlicDrivePickerOutcome::kUnknownError;
    case GlicDrivePickerError::kWebSessionSignedOut:
      return GlicDrivePickerOutcome::kWebSessionSignedOut;
    case GlicDrivePickerError::kChromeSignInRequired:
      return GlicDrivePickerOutcome::kChromeSignInRequired;
    case GlicDrivePickerError::kAccountMismatch:
      return GlicDrivePickerOutcome::kAccountMismatch;
    case GlicDrivePickerError::kAlreadyActive:
      return GlicDrivePickerOutcome::kAlreadyActive;
    case GlicDrivePickerError::kNoAnchorWindow:
      return GlicDrivePickerOutcome::kNoAnchorWindow;
    case GlicDrivePickerError::kLoadFailed:
      // Load failures have several causes. Callers pass the specific outcome
      // to `FinishRequest()` instead of going through here.
      NOTREACHED();
    case GlicDrivePickerError::kDisabledByPolicy:
      return GlicDrivePickerOutcome::kDisabledByPolicy;
    case GlicDrivePickerError::kUnsupported:
      return GlicDrivePickerOutcome::kUnsupported;
  }
}

// Like `OutcomeForError(DrivePickerErrorToReason(error))`, but keeps the
// distinct causes that the web client only sees as `kLoadFailed`.
GlicDrivePickerOutcome OutcomeForHostError(
    drive_picker_host::mojom::DrivePickerError error) {
  using drive_picker_host::mojom::DrivePickerError;
  switch (error) {
    case DrivePickerError::kTokenFetchFailure:
      return GlicDrivePickerOutcome::kTokenFetchFailed;
    case DrivePickerError::kApiLoadFailure:
      return GlicDrivePickerOutcome::kApiLoadFailed;
    case DrivePickerError::kMojoDisconnected:
      return GlicDrivePickerOutcome::kPickerDisconnected;
    case DrivePickerError::kInvalidResponse:
      return GlicDrivePickerOutcome::kInvalidResponse;
    case DrivePickerError::kInvalidFileId:
      return GlicDrivePickerOutcome::kInvalidFileId;
    case DrivePickerError::kInvalidFileMetadata:
      return GlicDrivePickerOutcome::kInvalidFileMetadata;
    case DrivePickerError::kUnsupportedFileType:
      return GlicDrivePickerOutcome::kUnsupportedFileType;
    case DrivePickerError::kInvalidFileSize:
      return GlicDrivePickerOutcome::kInvalidFileSize;
    case DrivePickerError::kWebUINotFound:
    case DrivePickerError::kViewNotFound:
      return GlicDrivePickerOutcome::kDialogCreationFailed;
    case DrivePickerError::kAlreadyActive:
      return GlicDrivePickerOutcome::kAlreadyActive;
    case DrivePickerError::kWindowNotFound:
      return GlicDrivePickerOutcome::kNoAnchorWindow;
    case DrivePickerError::kUnknown:
      return GlicDrivePickerOutcome::kUnknownError;
  }
}

void RecordOutcome(GlicDrivePickerOutcome outcome) {
  // Justification: The Drive picker is only supported for the consumer Gemini
  // provider, so there is nothing to split by provider.
  base::UmaHistogramEnumeration("Glic.DrivePicker.Outcome", outcome);
}

bool IsDisallowedByExplicitPolicyOrConsent(
    Profile* profile,
    const GlicEnabling::ProfileEnablement& enablement) {
  return !profile ||
         prefs::GetFileUploadAllowedCapability(profile->GetPrefs()) ==
             mojom::FileUploadPolicyState::kDisabled ||
         enablement.DisallowedByAdmin() || !enablement.fre_is_consented;
}

std::unique_ptr<DrivePickerHostController> CreateDefaultController(
    Profile* profile,
    BrowserWindowInterface* browser,
    views::Widget* anchor_widget) {
  return std::make_unique<DrivePickerHostController>(profile, browser,
                                                     anchor_widget);
}

BrowserWindowInterface* FindAnchorBrowserImpl(
    Profile* profile,
    views::Widget* anchor_widget,
    base::FunctionRef<bool(BrowserWindowInterface*)> is_browser_visible) {
  // Pick the best eligible browser window for `profile` in Z-order (most
  // recently active first):
  // 1. Skip windows scheduled for deletion, hidden/minimized windows, and
  //    windows that cannot host Glic (e.g. popups, apps, other profiles).
  // 2. Prefer the eligible browser window whose native window matches
  //    `anchor_widget` (the attached side-panel case) and stop searching.
  // 3. Otherwise remember the most recently active eligible browser window as a
  //    fallback (the detached floaty case when a browser window is open). If no
  //    eligible browser window exists, return nullptr so
  //    `DrivePickerHostController` anchors directly to `anchor_widget`.
  BrowserWindowInterface* browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* b) {
        if (!b->IsDeleteScheduled() && is_browser_visible(b) &&
            IsBrowserGlicAttachable(profile, b)) {
          if (anchor_widget && b->GetWindow()->GetNativeWindow() ==
                                   anchor_widget->GetNativeWindow()) {
            browser = b;
            return false;
          }
          if (!browser) {
            browser = b;
          }
        }
        return true;
      });
  return browser;
}

}  // namespace

GlicDrivePickerManager::GlicDrivePickerManager(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicEnabling* enabling)
    : profile_(profile),
      identity_manager_(identity_manager),
      controller_factory_(base::BindRepeating(&CreateDefaultController)) {
  if (profile_ && profile_->GetPrefs()) {
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kGlicFileUploadAllowed,
        base::BindRepeating(&GlicDrivePickerManager::OnPolicyOrEnablingChanged,
                            base::Unretained(this)));
  }
  if (enabling) {
    allowed_changed_subscription_ = enabling->RegisterAllowedChanged(
        base::BindRepeating(&GlicDrivePickerManager::OnPolicyOrEnablingChanged,
                            base::Unretained(this)));
    consent_changed_subscription_ = enabling->RegisterOnConsentChanged(
        base::BindRepeating(&GlicDrivePickerManager::OnPolicyOrEnablingChanged,
                            base::Unretained(this)));
  }
}

GlicDrivePickerManager::~GlicDrivePickerManager() {
  FinishRequest(std::vector<GlicDriveFileSelection>(),
                GlicDrivePickerOutcome::kCancelledByOwner);
}

void GlicDrivePickerManager::ShowDrivePicker(
    const base::UnguessableToken& owner_token,
    const GaiaId& expected_gaia,
    bool is_panel_showing,
    content::WebContents* webui_contents,
    ShowCallback callback) {
  if (std::optional<GlicDrivePickerError> error =
          ValidateRequest(expected_gaia, is_panel_showing)) {
    RecordOutcome(OutcomeForError(*error));
    std::move(callback).Run(base::unexpected(*error));
    return;
  }
  LaunchPicker(owner_token, expected_gaia, webui_contents, std::move(callback));
}

void GlicDrivePickerManager::CancelForOwner(
    const base::UnguessableToken& owner_token) {
  if (controller_ && active_owner_token_ == owner_token) {
    FinishRequest(std::vector<GlicDriveFileSelection>(),
                  GlicDrivePickerOutcome::kCancelledByOwner);
  }
}

// static
BrowserWindowInterface* GlicDrivePickerManager::FindAnchorBrowser(
    Profile* profile,
    views::Widget* anchor_widget) {
  return FindAnchorBrowserImpl(profile, anchor_widget, &IsBrowserVisible);
}

// static
BrowserWindowInterface* GlicDrivePickerManager::FindAnchorBrowserForTesting(
    Profile* profile,
    views::Widget* anchor_widget,
    base::FunctionRef<bool(BrowserWindowInterface*)> is_browser_visible) {
  return FindAnchorBrowserImpl(profile, anchor_widget, is_browser_visible);
}

void GlicDrivePickerManager::SetControllerFactoryForTesting(
    ControllerFactory factory) {
  controller_factory_ = std::move(factory);
}

std::optional<GlicDrivePickerError> GlicDrivePickerManager::ValidateRequest(
    const GaiaId& expected_gaia,
    bool is_panel_showing) const {
  if (!profile_ || !GetGlicProviderSpec(profile_).SupportsFeature(
                       GlicFeature::kDrivePicker)) {
    return GlicDrivePickerError::kUnsupported;
  }

  const GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile_);
  if (IsDisallowedByExplicitPolicyOrConsent(profile_, enablement)) {
    return GlicDrivePickerError::kDisabledByPolicy;
  }

  if (!is_panel_showing) {
    return GlicDrivePickerError::kNoAnchorWindow;
  }

  if (controller_) {
    return GlicDrivePickerError::kAlreadyActive;
  }

  // The picker uses the primary account's OAuth token and the profile's Google
  // cookies, so the web client must be signed in to the primary account and
  // that account's web session must be healthy. Check account readiness before
  // falling back to `!enablement.IsEnabledAndConsented()` so a signed-out
  // desktop profile (where `kGlicShowForSignedOut` is off) reports
  // `kChromeSignInRequired` instead of `kDisabledByPolicy`.
  if (std::optional<DrivePickerAccountError> account_error =
          CheckDrivePickerAccount(identity_manager_, expected_gaia)) {
    return AccountErrorToPickerError(*account_error);
  }

  if (!enablement.IsEnabledAndConsented()) {
    return GlicDrivePickerError::kDisabledByPolicy;
  }

  return std::nullopt;
}

std::optional<GlicDrivePickerError>
GlicDrivePickerManager::CheckActivePolicyAndEnabling() const {
  const GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile_);
  if (IsDisallowedByExplicitPolicyOrConsent(profile_, enablement)) {
    return GlicDrivePickerError::kDisabledByPolicy;
  }
  // Check primary account / token / Gaia match without calling
  // `GetAccountsInCookieJar()` so enabling updates never trigger
  // `/ListAccounts` and `GlicEnabling`'s earlier `IdentityManager` observer
  // registration does not misreport sign-out or account switches as
  // `kDisabledByPolicy`.
  if (std::optional<DrivePickerAccountError> account_error =
          CheckDrivePickerPrimaryAccount(identity_manager_,
                                         active_expected_gaia_)) {
    return AccountErrorToPickerError(*account_error);
  }
  if (!enablement.IsEnabledAndConsented()) {
    return GlicDrivePickerError::kDisabledByPolicy;
  }
  return std::nullopt;
}

void GlicDrivePickerManager::LaunchPicker(
    const base::UnguessableToken& owner_token,
    const GaiaId& expected_gaia,
    content::WebContents* webui_contents,
    ShowCallback callback) {
  active_owner_token_ = owner_token;
  active_expected_gaia_ = expected_gaia;
  callback_ = std::move(callback);

  // Resolve the top-level widget hosting the Glic WebUI first so we can prefer
  // anchoring to the browser window that actually owns this Glic side panel
  // (rather than an arbitrary active window), and fall back to anchoring to the
  // detached floating widget when no eligible browser window exists.
  views::Widget* anchor_widget = nullptr;
  if (webui_contents) {
    anchor_widget = views::Widget::GetTopLevelWidgetForNativeView(
        webui_contents->GetContentNativeView());
  }

  BrowserWindowInterface* browser = FindAnchorBrowser(profile_, anchor_widget);
  controller_ = controller_factory_.Run(profile_, browser, anchor_widget);

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      result_handler_receiver_.BindNewPipeAndPassRemote());
  request->set_style(drive_picker_host::DrivePickerHostRequest::Style::kGlic);

  result_handler_receiver_.set_disconnect_handler(base::BindOnce(
      &GlicDrivePickerManager::OnPickerDisconnected, base::Unretained(this)));

  controller_->set_on_close_callback(base::BindOnce(
      &GlicDrivePickerManager::OnPickerClosed, base::Unretained(this)));

  // Defense-in-depth: if the primary account's web session or token is
  // invalidated while the picker is open, close it and fail with the reason.
  account_watcher_ = std::make_unique<DrivePickerAccountWatcher>(
      identity_manager_, active_expected_gaia_,
      base::BindOnce(&GlicDrivePickerManager::OnAccountInvalidated,
                     base::Unretained(this)));

  controller_->ShowDrivePickerHost(std::move(request));
}

void GlicDrivePickerManager::OnSelection(
    std::vector<drive_picker_host::mojom::DriveFilePtr> files) {
  if (std::optional<GlicDrivePickerError> error =
          CheckActivePolicyAndEnabling()) {
    FinishRequestWithError(*error);
    return;
  }

  if (files.size() > kMaxSelections) {
    result_handler_receiver_.ReportBadMessage("Too many Drive files selected.");
    FinishRequest(base::unexpected(GlicDrivePickerError::kLoadFailed),
                  GlicDrivePickerOutcome::kSelectionRejectedByBrowser);
    return;
  }

  std::vector<GlicDriveFileSelection> result;
  result.reserve(files.size());
  for (const auto& file : files) {
    std::optional<SanitizedDriveFileData> sanitized =
        DrivePickerSanitizer::Sanitize(file);
    if (!sanitized) {
      continue;
    }
    GlicDriveFileSelection selection;
    selection.id = std::move(sanitized->drive_id);
    selection.title = std::move(sanitized->file_name);
    selection.mime_type = std::move(sanitized->mime_type);
    selection.size_bytes = sanitized->size_bytes;
    selection.resource_key = std::move(sanitized->resource_key);
    result.push_back(std::move(selection));
  }

  if (result.empty()) {
    FinishRequest(base::unexpected(GlicDrivePickerError::kLoadFailed),
                  GlicDrivePickerOutcome::kSelectionRejectedByBrowser);
    return;
  }

  FinishRequest(std::move(result), GlicDrivePickerOutcome::kFilesPicked);
}

void GlicDrivePickerManager::OnCancel() {
  FinishRequest(std::vector<GlicDriveFileSelection>(),
                GlicDrivePickerOutcome::kCancelledByUser);
}

void GlicDrivePickerManager::OnError(
    drive_picker_host::mojom::DrivePickerError error) {
  FinishRequest(base::unexpected(DrivePickerErrorToReason(error)),
                OutcomeForHostError(error));
}

void GlicDrivePickerManager::OnPickerDisconnected() {
  FinishRequest(base::unexpected(GlicDrivePickerError::kLoadFailed),
                GlicDrivePickerOutcome::kPickerDisconnected);
}

void GlicDrivePickerManager::OnPickerClosed() {
  FinishRequest(std::vector<GlicDriveFileSelection>(),
                GlicDrivePickerOutcome::kDialogClosed);
}

void GlicDrivePickerManager::OnAccountInvalidated(
    DrivePickerAccountError reason) {
  FinishRequestWithError(AccountErrorToPickerError(reason));
}

void GlicDrivePickerManager::OnPolicyOrEnablingChanged() {
  if (!controller_) {
    return;
  }
  if (std::optional<GlicDrivePickerError> error =
          CheckActivePolicyAndEnabling()) {
    FinishRequestWithError(*error);
  }
}

void GlicDrivePickerManager::FinishRequest(
    base::expected<std::vector<GlicDriveFileSelection>, GlicDrivePickerError>
        result,
    GlicDrivePickerOutcome outcome) {
  ShowCallback callback = std::move(callback_);
  Cleanup();
  if (callback) {
    RecordOutcome(outcome);
    std::move(callback).Run(std::move(result));
  }
}

void GlicDrivePickerManager::FinishRequestWithError(
    GlicDrivePickerError error) {
  FinishRequest(base::unexpected(error), OutcomeForError(error));
}

void GlicDrivePickerManager::Cleanup() {
  active_owner_token_ = base::UnguessableToken();
  active_expected_gaia_ = GaiaId();
  account_watcher_.reset();
  result_handler_receiver_.reset();
  if (controller_) {
    // Clear the close callback before destroying the controller so
    // `~DrivePickerHostController` does not re-enter `OnPickerClosed`.
    controller_->set_on_close_callback(base::NullCallback());
    // Destroying the controller closes the picker dialog widget.
    controller_.reset();
  }
}

}  // namespace glic
