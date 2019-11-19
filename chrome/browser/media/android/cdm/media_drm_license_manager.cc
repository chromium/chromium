// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_license_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/unguessable_token.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "media/base/android/media_drm_bridge.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#include "url/origin.h"

namespace {
// Unprovision MediaDrm in IO thread.
void ClearMediaDrmLicensesBlocking(
    std::vector<base::UnguessableToken> origin_ids) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  for (const auto& origin_id : origin_ids) {
    // MediaDrm will unprovision |origin_id| for all security level. Passing
    // DEFAULT here is OK.
    scoped_refptr<media::MediaDrmBridge> media_drm_bridge =
        media::MediaDrmBridge::CreateWithoutSessionSupport(
            kWidevineKeySystem, origin_id.ToString(),
            media::MediaDrmBridge::SECURITY_LEVEL_DEFAULT,
            media::CreateFetcherCB());

    DCHECK(media_drm_bridge);

    media_drm_bridge->Unprovision();
  }
}
}  // namespace

void ClearMediaDrmLicenses(
    PrefService* prefs,
    base::Time delete_begin,
    base::Time delete_end,
    const base::RepeatingCallback<bool(const GURL& url)>& filter,
    base::OnceClosure complete_cb) {
  // Clear persisted license meta data in |prefs|.
  std::vector<base::UnguessableToken> no_license_origin_ids =
      cdm::MediaDrmStorageImpl::ClearMatchingLicenses(prefs, delete_begin,
                                                      delete_end, filter);

  if (no_license_origin_ids.empty()) {
    std::move(complete_cb).Run();
    return;
  }

  // Create a single thread task runner for MediaDrmBridge, for posting Java
  // callbacks immediately to avoid rentrancy issues.
  // TODO(yucliu): Remove task runner from MediaDrmBridge in this case.
  base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock()})
      ->PostTaskAndReply(FROM_HERE,
                         base::BindOnce(&ClearMediaDrmLicensesBlocking,
                                        std::move(no_license_origin_ids)),
                         std::move(complete_cb));
}
