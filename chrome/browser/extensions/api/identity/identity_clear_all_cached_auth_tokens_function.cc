// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_clear_all_cached_auth_tokens_function.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace extensions {

namespace {

constexpr WebAuthFlow::Partition kPartitionsToClean[] = {
    WebAuthFlow::GET_AUTH_TOKEN, WebAuthFlow::LAUNCH_WEB_AUTH_FLOW};

}

IdentityClearAllCachedAuthTokensFunction::
    IdentityClearAllCachedAuthTokensFunction() = default;
IdentityClearAllCachedAuthTokensFunction::
    ~IdentityClearAllCachedAuthTokensFunction() = default;

ExtensionFunction::ResponseAction
IdentityClearAllCachedAuthTokensFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(identity_constants::kOffTheRecord));

  IdentityAPI* id_api = IdentityAPI::GetFactoryInstance()->Get(profile);
  id_api->EraseGaiaIdForExtension(extension()->id());
  id_api->token_cache()->EraseAllTokensForExtension(extension()->id());

  for (WebAuthFlow::Partition partition : kPartitionsToClean) {
    profile
        ->GetStoragePartition(
            WebAuthFlow::GetWebViewPartitionConfig(partition, profile))
        ->GetCookieManagerForBrowserProcess()
        ->DeleteCookies(
            network::mojom::CookieDeletionFilter::New(),
            base::BindOnce(
                &IdentityClearAllCachedAuthTokensFunction::OnCookiesDeleted,
                this));
  }

  // This object is retained by the DeleteCookies callbacks.
  return RespondLater();
}

void IdentityClearAllCachedAuthTokensFunction::OnCookiesDeleted(
    uint32_t num_deleted) {
  ++cleaned_partitions_;

  if (cleaned_partitions_ < std::size(kPartitionsToClean))
    return;

  // Post a task to ensure Respond() is not synchronously called from Run(). The
  // object is retained by this task.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&IdentityClearAllCachedAuthTokensFunction::Respond, this,
                     NoArguments()));
}

}  // namespace extensions
