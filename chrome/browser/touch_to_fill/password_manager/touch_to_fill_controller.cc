// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_controller_delegate.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_view_factory.h"
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
using DisplayTarget = TouchToFillController::DisplayTarget;

std::vector<UiCredential> SortCredentials(
    base::span<const UiCredential> credentials) {
  std::vector<UiCredential> result(credentials.begin(), credentials.end());
  // Sort `credentials` according to the following criteria:
  // 1) Prefer exact matches then affiliated, then PSL matches.
  // 2) Prefer credentials that were used recently over others.
  //
  // Note: This ordering matches password_manager_util::FindBestMatches().
  base::ranges::sort(result, std::greater<>{}, [](const UiCredential& cred) {
    return std::make_pair(-static_cast<int>(cred.match_type()),
                          cred.last_used());
  });

  return result;
}
}  // namespace

TouchToFillController::TouchToFillController(
    Profile* profile,
    base::WeakPtr<
        password_manager::KeyboardReplacingSurfaceVisibilityController>
        visibility_controller)
    : profile_(profile), visibility_controller_(visibility_controller) {}
TouchToFillController::~TouchToFillController() = default;

bool TouchToFillController::Show(
    base::span<const UiCredential> credentials,
    base::span<PasskeyCredential> passkey_credentials,
    std::unique_ptr<TouchToFillControllerDelegate> ttf_delegate,
    webauthn::WebAuthnCredManDelegate* cred_man_delegate,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver>
        frame_driver) {
  if (!ttf_delegate->ShouldShowTouchToFill()) {
    return false;
  }

  DCHECK(!ttf_delegate_);
  ttf_delegate_ = std::move(ttf_delegate);

  cred_man_delegate_ = cred_man_delegate;
  visibility_controller_->SetVisible(std::move(frame_driver));

  ttf_delegate_->OnShow(credentials, passkey_credentials);
  GURL url = ttf_delegate_->GetFrameUrl();
  // If the render frame host has been destroyed already, the url will be empty
  // in which case Show() should never be called.
  CHECK(!url.is_empty());

  switch (GetResponsibleDisplayTarget(credentials, passkey_credentials)) {
    case DisplayTarget::kNone:
      // Ideally this should never happen. However, in case we do end up
      // invoking Show() without credentials, we should not show Touch To Fill
      // to the user and treat this case as dismissal, in order to restore the
      // soft keyboard.
      OnDismiss();
      return false;
    case DisplayTarget::kShowNoPasskeysSheet:
      if (!no_passkeys_bridge_) {
        no_passkeys_bridge_ = std::make_unique<NoPasskeysBottomSheetBridge>();
      }
      no_passkeys_bridge_->Show(
          GetNativeView()->GetWindowAndroid(), url::Origin::Create(url).host(),
          base::BindOnce(&TouchToFillController::OnDismiss,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&TouchToFillController::OnHybridSignInSelected,
                         weak_ptr_factory_.GetWeakPtr()));
      return true;
    case DisplayTarget::kDeferToCredMan:
      cred_man_delegate->SetRequestCompletionCallback(
          base::BindRepeating(&TouchToFillController::OnCredManUiClosed,
                              weak_ptr_factory_.GetWeakPtr()));
      OnShowCredManSelected();
      return true;
    case DisplayTarget::kShowTouchToFill:
      if (!view_) {
        view_ = TouchToFillViewFactory::Create(this);
      }

      int flags = TouchToFillView::kNone;

      if (ttf_delegate_->ShouldTriggerSubmission()) {
        flags |= TouchToFillView::kTriggerSubmission;
      }
      if (password_manager_launcher::CanManagePasswordsWhenPasskeysPresent(
              profile_)) {
        flags |= TouchToFillView::kCanManagePasswordsWhenPasskeysPresent;
      }
      if (ttf_delegate_->ShouldShowHybridOption()) {
        flags |= TouchToFillView::kShouldShowHybridOption;
      }
      if (cred_man_delegate &&
          cred_man_delegate->HasPasskeys() ==
              WebAuthnCredManDelegate::State::kHasPasskeys) {
        cred_man_delegate->SetRequestCompletionCallback(
            base::BindRepeating(&TouchToFillController::OnCredManUiClosed,
                                weak_ptr_factory_.GetWeakPtr()));
        flags |= TouchToFillView::kShouldShowCredManEntry;
      }

      return view_->Show(url,
                         TouchToFillView::IsOriginSecure(
                             network::IsOriginPotentiallyTrustworthy(
                                 url::Origin::Create(url))),
                         SortCredentials(credentials), passkey_credentials,
                         flags);
  }
}

void TouchToFillController::OnCredentialSelected(
    const UiCredential& credential) {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  ttf_delegate_->OnCredentialSelected(
      credential, base::BindOnce(&TouchToFillController::ActionCompleted,
                                 base::Unretained(this)));
}

void TouchToFillController::OnPasskeyCredentialSelected(
    const PasskeyCredential& credential) {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  ttf_delegate_->OnPasskeyCredentialSelected(
      credential, base::BindOnce(&TouchToFillController::ActionCompleted,
                                 base::Unretained(this)));
}

void TouchToFillController::OnManagePasswordsSelected(bool passkeys_shown) {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  ttf_delegate_->OnManagePasswordsSelected(
      passkeys_shown, base::BindOnce(&TouchToFillController::ActionCompleted,
                                     base::Unretained(this)));
}

void TouchToFillController::OnHybridSignInSelected() {
  view_.reset();
  ttf_delegate_->OnHybridSignInSelected(base::BindOnce(
      &TouchToFillController::ActionCompleted, base::Unretained(this)));
}

void TouchToFillController::OnShowCredManSelected() {
  view_.reset();
  cred_man_delegate_->TriggerCredManUi(
      WebAuthnCredManDelegate::RequestPasswords(false));
}

void TouchToFillController::OnCredManUiClosed(bool success) {
  if (!ttf_delegate_) {
    return;
  }
  // Unretained is safe here because TouchToFillController owns the delegate.
  ttf_delegate_->OnCredManDismissed(base::BindOnce(
      &TouchToFillController::ActionCompleted, base::Unretained(this)));
}

void TouchToFillController::OnDismiss() {
  view_.reset();
  no_passkeys_bridge_.reset();
  if (!ttf_delegate_) {
    // TODO(crbug.com/40274966): Remove this check when
    // PasswordSuggestionBottomSheetV2 is launched
    return;
  }
  // Unretained is safe here because TouchToFillController owns the delegate.
  ttf_delegate_->OnDismiss(base::BindOnce(
      &TouchToFillController::ActionCompleted, base::Unretained(this)));
}

Profile* TouchToFillController::GetProfile() {
  return profile_;
}

gfx::NativeView TouchToFillController::GetNativeView() {
  return ttf_delegate_->GetNativeView();
}

void TouchToFillController::Close() {
  // TODO(crbug.com/40277147). This is a duplicate of `OnDismiss`. Merge the two
  // functions.
  OnDismiss();
}

void TouchToFillController::Reset() {
  if (!visibility_controller_) {
    return;
  }
  if (visibility_controller_->IsVisible()) {
    Close();
  }
  visibility_controller_->Reset();
}

void TouchToFillController::ActionCompleted() {
  if (visibility_controller_) {
    visibility_controller_->SetShown();
  }
  ttf_delegate_.reset();
}

DisplayTarget TouchToFillController::GetResponsibleDisplayTarget(
    base::span<const password_manager::UiCredential> credentials,
    base::span<password_manager::PasskeyCredential> passkey_credentials) const {
  bool has_passkeys_in_cred_man =
      cred_man_delegate_ && cred_man_delegate_->HasPasskeys() ==
                                WebAuthnCredManDelegate::State::kHasPasskeys;
  if (!credentials.empty() || !passkey_credentials.empty()) {
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
