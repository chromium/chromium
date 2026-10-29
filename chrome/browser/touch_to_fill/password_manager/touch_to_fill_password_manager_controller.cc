// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_password_manager_controller.h"

#include <algorithm>
#include <tuple>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_bridge.h"
#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"
#include "chrome/browser/password_manager/android/password_manager_ui_util_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_password_manager_delegate.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_password_manager_view.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_password_manager_view_factory.h"
#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/url_formatter/elide_url.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "ui/android/view_android.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
using password_manager::PasskeyCredential;
using password_manager::UiCredential;
using webauthn::WebAuthnCredManDelegate;
using Credential = TouchToFillPasswordManagerView::Credential;
using DisplayTarget = TouchToFillPasswordManagerController::DisplayTarget;

// Constants used to rank passkeys and passwords in the default credential
// sorting. Passkeys have a higher value so they will be sorted above
// passwords.
constexpr int kPasskeySortValue = 1;
constexpr int kPasswordSortValue = 0;
// `kPlaceholderSortValue` is an arbitrary value for credential type, which
// is not used for passkeys.
constexpr int kPlaceholderSortValue = 0;

std::vector<Credential> DefaultSortCredentials(
    base::span<const Credential> credentials) {
  std::vector<Credential> result(credentials.begin(), credentials.end());
  // Sort `credentials` according to the following criteria:
  // 1) List passkeys before passwords.
  // 2) List exact matches first, then affiliated, then PSL matches.
  // 3) List credentials that were used recently before others.
  // 4) List recovery passwords immediately after their corresponding main
  //    password (note: main and recovery passwords have the same last used
  //    date).
  //
  // Note: This ordering matches password_manager_util::FindBestMatches(),
  // apart from the backup password which only exists separately in the UI.
  std::ranges::sort(result, std::greater<>{}, [](const Credential& cred) {
    if (const auto* passkey = std::get_if<PasskeyCredential>(&cred)) {
      return std::make_tuple(
          kPasskeySortValue, kPlaceholderSortValue,
          passkey->last_used_time().value_or(base::Time::Min()), false);
    }
    const auto* password = &std::get<UiCredential>(cred);
    return std::make_tuple(
        kPasswordSortValue, -static_cast<int>(password->match_type()),
        password->last_used(), !password->is_backup_credential());
  });

  return result;
}
}  // namespace

TouchToFillPasswordManagerController::TouchToFillPasswordManagerController(
    Profile* profile,
    base::WeakPtr<
        password_manager::KeyboardReplacingSurfaceVisibilityController>
        visibility_controller,
    std::unique_ptr<AcknowledgeGroupedCredentialSheetController>
        grouped_credential_sheet_controller)
    : profile_(profile),
      visibility_controller_(visibility_controller),
      grouped_credential_sheet_controller_(
          std::move(grouped_credential_sheet_controller)) {}
TouchToFillPasswordManagerController::~TouchToFillPasswordManagerController() =
    default;

void TouchToFillPasswordManagerController::InitData(
    std::vector<Credential> credentials,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver>
        frame_driver) {
  credentials_ = std::move(credentials);
  frame_driver_ = frame_driver;
}

bool TouchToFillPasswordManagerController::Show(
    std::unique_ptr<TouchToFillPasswordManagerDelegate> ttf_delegate,
    webauthn::WebAuthnCredManDelegate* cred_man_delegate) {
  if (!ttf_delegate->ShouldShowTouchToFill()) {
    return false;
  }

  CHECK(!ttf_delegate_, base::NotFatalUntil::M163);
  ttf_delegate_ = std::move(ttf_delegate);

  cred_man_delegate_ = cred_man_delegate;
  visibility_controller_->SetVisible(frame_driver_);

  ttf_delegate_->OnShow(credentials_);
  GURL url = ttf_delegate_->GetFrameUrl();
  // If the render frame host has been destroyed already, the url will be empty
  // in which case Show() should never be called.
  CHECK(!url.is_empty());
  url::Origin origin = ttf_delegate_->GetFrameOrigin();

  switch (GetResponsibleDisplayTarget(credentials_)) {
    case DisplayTarget::kNone:
      // Ideally this should never happen. However, in case we do end up
      // invoking Show() without credentials, we should not show Touch To Fill
      // to the user and treat this case as dismissal, in order to restore the
      // soft keyboard.
      OnDismiss();
      return false;
    case DisplayTarget::kShowNoPasskeysSheet:
      if (!GetNativeView()->GetWindowAndroid()) {
        return false;  // Chrome exits and can't show a sheet anymore.
      }
      if (!no_passkeys_bridge_) {
        no_passkeys_bridge_ = std::make_unique<NoPasskeysBottomSheetBridge>();
      }
      no_passkeys_bridge_->Show(
          GetNativeView()->GetWindowAndroid(), origin.host(),
          base::BindOnce(&TouchToFillPasswordManagerController::OnDismiss,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &TouchToFillPasswordManagerController::OnHybridSignInSelected,
              weak_ptr_factory_.GetWeakPtr()));
      return true;
    case DisplayTarget::kDeferToCredMan:
      cred_man_delegate->SetRequestCompletionCallback(base::BindRepeating(
          &TouchToFillPasswordManagerController::OnCredManUiClosed,
          weak_ptr_factory_.GetWeakPtr()));
      OnShowCredManSelected();
      return true;
    case DisplayTarget::kShowTouchToFill:
      if (!view_) {
        view_ = TouchToFillPasswordManagerViewFactory::Create(this);
      }

      int flags = TouchToFillPasswordManagerView::kNone;

      if (ttf_delegate_->ShouldTriggerSubmission()) {
        flags |= TouchToFillPasswordManagerView::kTriggerSubmission;
      }
      if (ttf_delegate_->ShouldShowHybridOption()) {
        flags |= TouchToFillPasswordManagerView::kShouldShowHybridOption;
      }
      if (cred_man_delegate &&
          cred_man_delegate->HasPasskeys() ==
              WebAuthnCredManDelegate::State::kHasPasskeys) {
        cred_man_delegate->SetRequestCompletionCallback(base::BindRepeating(
            &TouchToFillPasswordManagerController::OnCredManUiClosed,
            weak_ptr_factory_.GetWeakPtr()));
        flags |= TouchToFillPasswordManagerView::kShouldShowCredManEntry;
      }

      std::optional<std::vector<Credential>> sorted_credentials =
          ttf_delegate_->SortCredentials(credentials_);
      if (!sorted_credentials.has_value()) {
        sorted_credentials = DefaultSortCredentials(credentials_);
      }

      return view_->Show(url,
                         TouchToFillPasswordManagerView::IsOriginSecure(
                             network::IsOriginPotentiallyTrustworthy(origin)),
                         *sorted_credentials, flags);
  }
}

void TouchToFillPasswordManagerController::OnCredentialSelected(
    const UiCredential& credential) {
  view_.reset();

  if (credential.match_type() ==
      password_manager_util::GetLoginMatchType::kGrouped) {
    std::string current_origin =
        GetDisplayOrigin(ttf_delegate_->GetFrameOrigin());
    // Use `cred->display_name()` instead of origin here to correctly display
    // credentials saved for android apps.
    grouped_credential_sheet_controller_->ShowAcknowledgeSheet(
        std::move(current_origin), credential.display_name(),
        GetNativeView()->GetWindowAndroid(),
        base::BindOnce(&TouchToFillPasswordManagerController::
                           OnAcknowledgementBeforeFillingReceived,
                       weak_ptr_factory_.GetWeakPtr(), credential));
    return;
  }

  // Emit UMA if grouped affiliation match was available for the user.
  if (std::ranges::find_if(credentials_, [](const Credential& login) {
        const UiCredential* password_login = std::get_if<UiCredential>(&login);
        return password_login &&
               password_login->match_type() ==
                   password_manager_util::GetLoginMatchType::kGrouped;
      }) != credentials_.end()) {
    password_manager::metrics_util::LogFillSuggestionGroupedMatchAccepted(
        /*grouped_match_accepted=*/false);
  }
  // A WeakPtr is necessary because the delegate may trigger this callback
  // during or after the destruction of this controller.
  ttf_delegate_->OnCredentialSelected(
      credential,
      base::BindOnce(&TouchToFillPasswordManagerController::ActionCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TouchToFillPasswordManagerController::
    OnAcknowledgementBeforeFillingReceived(
        const password_manager::UiCredential& credential,
        AcknowledgeGroupedCredentialSheetBridge::DismissReason dismiss_reason) {
  // Emit UMA if grouped affiliation match was available for the user.
  password_manager::metrics_util::LogFillSuggestionGroupedMatchAccepted(
      dismiss_reason ==
      AcknowledgeGroupedCredentialSheetBridge::DismissReason::kAccept);

  switch (dismiss_reason) {
    case AcknowledgeGroupedCredentialSheetBridge::DismissReason::kAccept:
      // A WeakPtr is necessary because the delegate may trigger this callback
      // during or after the destruction of this controller.
      ttf_delegate_->OnCredentialSelected(
          credential,
          base::BindOnce(&TouchToFillPasswordManagerController::ActionCompleted,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case AcknowledgeGroupedCredentialSheetBridge::DismissReason::kBack:
      visibility_controller_->SetCanBeShown();
      Show(std::move(ttf_delegate_), cred_man_delegate_);
      break;
    case AcknowledgeGroupedCredentialSheetBridge::DismissReason::kIgnore:
      // Do nothing here.
      break;
  }
}

void TouchToFillPasswordManagerController::OnPasskeyCredentialSelected(
    const PasskeyCredential& credential) {
  view_.reset();
  // A WeakPtr is necessary because the delegate may trigger this callback
  // during or after the destruction of this controller.
  ttf_delegate_->OnPasskeyCredentialSelected(
      credential,
      base::BindOnce(&TouchToFillPasswordManagerController::ActionCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TouchToFillPasswordManagerController::OnManagePasswordsSelected(
    bool passkeys_shown) {
  view_.reset();
  // A WeakPtr is necessary because the delegate may trigger this callback
  // during or after the destruction of this controller.
  ttf_delegate_->OnManagePasswordsSelected(
      passkeys_shown,
      base::BindOnce(&TouchToFillPasswordManagerController::ActionCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TouchToFillPasswordManagerController::OnHybridSignInSelected() {
  view_.reset();
  // A WeakPtr is necessary because the delegate may trigger this callback
  // during or after the destruction of this controller.
  ttf_delegate_->OnHybridSignInSelected(
      base::BindOnce(&TouchToFillPasswordManagerController::ActionCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TouchToFillPasswordManagerController::OnShowCredManSelected() {
  view_.reset();
  cred_man_delegate_->TriggerCredManUi(
      WebAuthnCredManDelegate::RequestPasswords(false));
}

void TouchToFillPasswordManagerController::OnCredManUiClosed(bool success) {
  if (!ttf_delegate_) {
    return;
  }
  // A WeakPtr is necessary because the delegate may trigger this callback
  // during or after the destruction of this controller.
  ttf_delegate_->OnCredManDismissed(
      base::BindOnce(&TouchToFillPasswordManagerController::ActionCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TouchToFillPasswordManagerController::OnDismiss() {
  view_.reset();
  no_passkeys_bridge_.reset();
  if (!ttf_delegate_) {
    // TODO(crbug.com/40274966): Remove this check when
    // PasswordSuggestionBottomSheetV2 is launched
    return;
  }
  // A WeakPtr is necessary because the delegate may trigger this callback
  // during or after the destruction of this controller.
  ttf_delegate_->OnDismiss(
      base::BindOnce(&TouchToFillPasswordManagerController::ActionCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

Profile* TouchToFillPasswordManagerController::GetProfile() {
  return profile_;
}

gfx::NativeView TouchToFillPasswordManagerController::GetNativeView() {
  return ttf_delegate_->GetNativeView();
}

void TouchToFillPasswordManagerController::Close() {
  // TODO(crbug.com/40277147). This is a duplicate of `OnDismiss`. Merge the two
  // functions.
  OnDismiss();
}

void TouchToFillPasswordManagerController::Reset() {
  if (!visibility_controller_) {
    return;
  }
  if (visibility_controller_->IsVisible()) {
    Close();
  }
  visibility_controller_->Reset();
  credentials_.clear();
}

void TouchToFillPasswordManagerController::ActionCompleted() {
  if (visibility_controller_) {
    visibility_controller_->SetShown();
  }
  ttf_delegate_.reset();
}

DisplayTarget TouchToFillPasswordManagerController::GetResponsibleDisplayTarget(
    base::span<const Credential> credentials) const {
  bool has_passkeys_in_cred_man =
      cred_man_delegate_ && cred_man_delegate_->HasPasskeys() ==
                                WebAuthnCredManDelegate::State::kHasPasskeys;
  if (!credentials.empty()) {
    return DisplayTarget::kShowTouchToFill;
  }

  if (has_passkeys_in_cred_man) {
    return DisplayTarget::kDeferToCredMan;
  }
  if (ttf_delegate_->ShouldShowNoPasskeysSheetIfRequired()) {
    return DisplayTarget::kShowNoPasskeysSheet;
  }
  return DisplayTarget::kNone;
}
