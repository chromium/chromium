// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/platform_verification_impl.h"

#include <utility>

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
#include "chrome/browser/media/cdm_storage_id.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#endif

using media::mojom::PlatformVerification;

namespace {

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
// Only support version 1 of Storage Id. However, the "latest" version can also
// be requested.
const uint32_t kRequestLatestStorageIdVersion = 0;
const uint32_t kCurrentStorageIdVersion = 1;

std::vector<uint8_t> GetStorageIdSaltFromProfile(
    content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  Profile* profile =
      Profile::FromBrowserContext(rfh->GetProcess()->GetBrowserContext());
  return MediaStorageIdSalt::GetSalt(profile->GetPrefs());
}

#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)

}  // namespace

// static
void PlatformVerificationImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::PlatformVerification> receiver) {
  DVLOG(2) << __func__;
  DCHECK(render_frame_host);

  // PlatformVerificationFlow requires to run on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See FrameServiceBase for details.
  new PlatformVerificationImpl(render_frame_host, std::move(receiver));
}

PlatformVerificationImpl::PlatformVerificationImpl(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::PlatformVerification> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      render_frame_host_(render_frame_host) {}

PlatformVerificationImpl::~PlatformVerificationImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PlatformVerificationImpl::ChallengePlatform(
    const std::string& service_id,
    const std::string& challenge,
    ChallengePlatformCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

// TODO(crbug.com/676224). This should be commented out at the mojom
// level so that it's only available for ChromeOS.

#if defined(OS_CHROMEOS)
  if (!platform_verification_flow_)
    platform_verification_flow_ =
        base::MakeRefCounted<chromeos::attestation::PlatformVerificationFlow>();

  platform_verification_flow_->ChallengePlatformKey(
      web_contents(), service_id, challenge,
      base::Bind(&PlatformVerificationImpl::OnPlatformChallenged,
                 weak_factory_.GetWeakPtr(), base::Passed(&callback)));
#else
  // Not supported, so return failure.
  std::move(callback).Run(false, std::string(), std::string(), std::string());
#endif  // defined(OS_CHROMEOS)
}

#if defined(OS_CHROMEOS)
void PlatformVerificationImpl::OnPlatformChallenged(
    ChallengePlatformCallback callback,
    PlatformVerificationResult result,
    const std::string& signed_data,
    const std::string& signature,
    const std::string& platform_key_certificate) {
  DVLOG(2) << __func__ << ": " << result;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != chromeos::attestation::PlatformVerificationFlow::SUCCESS) {
    DCHECK(signed_data.empty());
    DCHECK(signature.empty());
    DCHECK(platform_key_certificate.empty());
    LOG(ERROR) << "Platform verification failed.";
    std::move(callback).Run(false, "", "", "");
    return;
  }

  DCHECK(!signed_data.empty());
  DCHECK(!signature.empty());
  DCHECK(!platform_key_certificate.empty());
  std::move(callback).Run(true, signed_data, signature,
                          platform_key_certificate);
}
#endif  // defined(OS_CHROMEOS)

void PlatformVerificationImpl::GetStorageId(uint32_t version,
                                            GetStorageIdCallback callback) {
  DVLOG(2) << __func__ << " version: " << version;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

// TODO(crbug.com/676224). This should be commented out at the mojom
// level so that it's only available if Storage Id is available.

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
  // Check that the request is for a supported version.
  if (version == kCurrentStorageIdVersion ||
      version == kRequestLatestStorageIdVersion) {
    ComputeStorageId(
        GetStorageIdSaltFromProfile(render_frame_host_), origin(),
        base::BindOnce(&PlatformVerificationImpl::OnStorageIdResponse,
                       weak_factory_.GetWeakPtr(), base::Passed(&callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)

  // Version not supported, so no Storage Id to return.
  DVLOG(2) << __func__ << " not supported";
  std::move(callback).Run(version, std::vector<uint8_t>());
}

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
void PlatformVerificationImpl::OnStorageIdResponse(
    GetStorageIdCallback callback,
    const std::vector<uint8_t>& storage_id) {
  DVLOG(2) << __func__ << " version: " << kCurrentStorageIdVersion
           << ", size: " << storage_id.size();

  std::move(callback).Run(kCurrentStorageIdVersion, storage_id);
}
#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)
