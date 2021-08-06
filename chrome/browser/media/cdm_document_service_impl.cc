// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_document_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "build/chromeos_buildflags.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
#include "chrome/browser/media/cdm_storage_id.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "content/public/browser/render_process_host.h"
#endif

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID) || defined(OS_CHROMEOS)
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if defined(OS_CHROMEOS)
#include "chrome/browser/media/platform_verification_chromeos.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/media/cdm_pref_service_helper.h"
#endif  // defined(OS_WIN)

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
void CdmDocumentServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::CdmDocumentService> receiver) {
  DVLOG(2) << __func__;
  DCHECK(render_frame_host);

  // PlatformVerificationFlow and the pref service requires to be run/accessed
  // on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentServiceBase for details.
  new CdmDocumentServiceImpl(render_frame_host, std::move(receiver));
}

CdmDocumentServiceImpl::CdmDocumentServiceImpl(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::CdmDocumentService> receiver)
    : DocumentServiceBase(render_frame_host, std::move(receiver)),
      render_frame_host_(render_frame_host) {}

CdmDocumentServiceImpl::~CdmDocumentServiceImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CdmDocumentServiceImpl::ChallengePlatform(
    const std::string& service_id,
    const std::string& challenge,
    ChallengePlatformCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/676224). This should be commented out at the mojom
  // level so that it's only available for ChromeOS.

#if defined(OS_CHROMEOS)
  bool success = platform_verification::PerformBrowserChecks(
      content::WebContents::FromRenderFrameHost(render_frame_host()));
  if (!success) {
    std::move(callback).Run(false, std::string(), std::string(), std::string());
    return;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service->IsAvailable<crosapi::mojom::ContentProtection>() &&
      lacros_service->GetInterfaceVersion(
          crosapi::mojom::ContentProtection::Uuid_) >=
          static_cast<int>(crosapi::mojom::ContentProtection::
                               kChallengePlatformMinVersion)) {
    lacros_service->GetRemote<crosapi::mojom::ContentProtection>()
        ->ChallengePlatform(
            service_id, challenge,
            base::BindOnce(&CdmDocumentServiceImpl::OnPlatformChallenged,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!platform_verification_flow_)
    platform_verification_flow_ =
        base::MakeRefCounted<ash::attestation::PlatformVerificationFlow>();

  platform_verification_flow_->ChallengePlatformKey(
      content::WebContents::FromRenderFrameHost(render_frame_host()),
      service_id, challenge,
      base::BindOnce(&CdmDocumentServiceImpl::OnPlatformChallenged,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
#else
  // Not supported, so return failure.
  std::move(callback).Run(false, std::string(), std::string(), std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void CdmDocumentServiceImpl::OnPlatformChallenged(
    ChallengePlatformCallback callback,
    PlatformVerificationResult result,
    const std::string& signed_data,
    const std::string& signature,
    const std::string& platform_key_certificate) {
  DVLOG(2) << __func__ << ": " << result;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != ash::attestation::PlatformVerificationFlow::SUCCESS) {
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void CdmDocumentServiceImpl::OnPlatformChallenged(
    ChallengePlatformCallback callback,
    crosapi::mojom::ChallengePlatformResultPtr result) {
  if (!result) {
    LOG(ERROR) << "Platform verification failed.";
    std::move(callback).Run(false, "", "", "");
    return;
  }
  std::move(callback).Run(true, std::move(result->signed_data),
                          std::move(result->signed_data_signature),
                          std::move(result->platform_key_certificate));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void CdmDocumentServiceImpl::GetStorageId(uint32_t version,
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
        base::BindOnce(&CdmDocumentServiceImpl::OnStorageIdResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)

  // Version not supported, so no Storage Id to return.
  DVLOG(2) << __func__ << " not supported";
  std::move(callback).Run(version, std::vector<uint8_t>());
}

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
void CdmDocumentServiceImpl::OnStorageIdResponse(
    GetStorageIdCallback callback,
    const std::vector<uint8_t>& storage_id) {
  DVLOG(2) << __func__ << " version: " << kCurrentStorageIdVersion
           << ", size: " << storage_id.size();

  std::move(callback).Run(kCurrentStorageIdVersion, storage_id);
}
#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)

#if defined(OS_CHROMEOS)
void CdmDocumentServiceImpl::IsVerifiedAccessEnabled(
    IsVerifiedAccessEnabledCallback callback) {
  // If we are in guest/incognito mode, then verified access is effectively
  // disabled.
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host_->GetBrowserContext());
  if (profile->IsOffTheRecord() || profile->IsGuestSession()) {
    std::move(callback).Run(false);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service->IsAvailable<crosapi::mojom::ContentProtection>() &&
      lacros_service->GetInterfaceVersion(
          crosapi::mojom::ContentProtection::Uuid_) >=
          static_cast<int>(crosapi::mojom::ContentProtection::
                               kIsVerifiedAccessEnabledMinVersion)) {
    lacros_service->GetRemote<crosapi::mojom::ContentProtection>()
        ->IsVerifiedAccessEnabled(std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
  bool enabled_for_device = false;
  ash::CrosSettings::Get()->GetBoolean(
      chromeos::kAttestationForContentProtectionEnabled, &enabled_for_device);
  std::move(callback).Run(enabled_for_device);
#endif  // else BUILDFLAG(IS_CHROMEOS_LACROS)
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
void CdmDocumentServiceImpl::GetCdmPreferenceData(
    GetCdmPreferenceDataCallback callback) {
  const url::Origin cdm_origin = origin();
  if (cdm_origin.opaque()) {
    mojo::ReportBadMessage("EME use is not allowed on opaque origin");
    return;
  }

  PrefService* user_prefs = user_prefs::UserPrefs::Get(
      content::WebContents::FromRenderFrameHost(render_frame_host())
          ->GetBrowserContext());

  std::move(callback).Run(
      CdmPrefServiceHelper::GetCdmPreferenceData(user_prefs, cdm_origin));
}

void CdmDocumentServiceImpl::SetCdmClientToken(
    const std::vector<uint8_t>& client_token) {
  const url::Origin cdm_origin = origin();
  if (cdm_origin.opaque()) {
    mojo::ReportBadMessage("EME use is not allowed on opaque origin");
    return;
  }

  PrefService* user_prefs = user_prefs::UserPrefs::Get(
      content::WebContents::FromRenderFrameHost(render_frame_host())
          ->GetBrowserContext());
  CdmPrefServiceHelper::SetCdmClientToken(user_prefs, cdm_origin, client_token);
}
#endif  // defined(OS_WIN)
