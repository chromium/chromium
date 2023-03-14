// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_api_lacros.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

namespace quick_unlock_private = api::quick_unlock_private;

// quickUnlockPrivate.getAuthToken

QuickUnlockPrivateGetAuthTokenFunction::
    QuickUnlockPrivateGetAuthTokenFunction() {}

QuickUnlockPrivateGetAuthTokenFunction::
    ~QuickUnlockPrivateGetAuthTokenFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetAuthTokenFunction::Run() {
  absl::optional<quick_unlock_private::GetAuthToken::Params> params =
      quick_unlock_private::GetAuthToken::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::Authentication>()) {
    // Bind |this| directly to utilize ExtensionFunction's ref-counting.
    auto crosapi_callback = base::BindOnce(
        &QuickUnlockPrivateGetAuthTokenFunction::OnCrosapiResult, this);
    lacros_service->GetRemote<crosapi::mojom::Authentication>()
        ->CreateQuickUnlockPrivateTokenInfo(params->account_password,
                                            std::move(crosapi_callback));
    return RespondLater();
  }
  return RespondNow(Error("Authentication crosapi unavailable."));
}

void QuickUnlockPrivateGetAuthTokenFunction::OnCrosapiResult(
    crosapi::mojom::CreateQuickUnlockPrivateTokenInfoResultPtr result) {
  using crosapi::mojom::CreateQuickUnlockPrivateTokenInfoResult;

  if (result->which() ==
      CreateQuickUnlockPrivateTokenInfoResult::Tag::kErrorMessage) {
    Respond(Error(result->get_error_message()));
  } else {
    DCHECK_EQ(result->which(),
              CreateQuickUnlockPrivateTokenInfoResult::Tag::kTokenInfo);
    auto in_token_info = std::move(result->get_token_info());
    auto out_token_info = std::make_unique<quick_unlock_private::TokenInfo>();
    out_token_info->token = in_token_info->token;
    out_token_info->lifetime_seconds = in_token_info->lifetime_seconds;
    Respond(ArgumentList(
        quick_unlock_private::GetAuthToken::Results::Create(*out_token_info)));
  }
}

}  // namespace extensions
