// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_document_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
#include "chrome/browser/media/cdm_storage_id.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "content/public/browser/render_process_host.h"
#endif

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/media/platform_verification_chromeos.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "chrome/browser/media/cdm_pref_service_helper.h"
#include "chrome/browser/media/media_foundation_service_monitor.h"
#include "media/cdm/media_foundation_cdm_data.h"
#include "media/cdm/win/media_foundation_cdm.h"
#include "sandbox/policy/win/lpac_capability.h"
#endif  // BUILDFLAG(IS_WIN)

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

#if BUILDFLAG(IS_WIN)
const char kCdmStore[] = "MediaFoundationCdmStore";

base::FilePath GetCdmStorePathRootForProfile(
    const base::FilePath& profile_path) {
  return profile_path.AppendASCII(kCdmStore).AppendASCII(
      base::SysInfo::ProcessCPUArchitecture());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

#if BUILDFLAG(IS_WIN)
bool CreateCdmStorePathRootAndGrantAccessIfNeeded(
    const base::FilePath& cdm_store_path_root) {
  if (!media::MediaFoundationCdm::IsAvailable()) {
    DLOG(ERROR) << "Granting access to LPAC process is not supported prior to "
                   "Windows 10.";
    return false;
  }
  // If the path exist, we can assume the right permission are already
  // set on it.
  if (base::PathExists(cdm_store_path_root))
    return true;

  base::File::Error file_error;
  if (!base::CreateDirectoryAndGetError(cdm_store_path_root, &file_error)) {
    DLOG(ERROR) << "Create CDM store path failed with " << file_error;
    return false;
  }

  auto sids = base::win::Sid::FromNamedCapabilityVector(
      {sandbox::policy::kMediaFoundationCdmData});
  return base::win::GrantAccessToPath(
      cdm_store_path_root, sids,
      FILE_GENERIC_READ | FILE_GENERIC_WRITE | GENERIC_EXECUTE | DELETE,
      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
}

std::unique_ptr<media::MediaFoundationCdmData>
GetMediaFoundationCdmDataInternal(const base::FilePath profile_path,
                                  std::unique_ptr<CdmPrefData> pref_data) {
  DCHECK(pref_data);

  auto cdm_store_path_root = GetCdmStorePathRootForProfile(profile_path);
  if (!CreateCdmStorePathRootAndGrantAccessIfNeeded(cdm_store_path_root)) {
    return nullptr;
  }

  std::unique_ptr<media::MediaFoundationCdmData> cdm_data;
  return std::make_unique<media::MediaFoundationCdmData>(
      pref_data->origin_id(), pref_data->client_token(), cdm_store_path_root);
}
#endif  // BUILDFLAG(IS_WIN)

// static
void CdmDocumentServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::CdmDocumentService> receiver) {
  DVLOG(2) << __func__;
  CHECK(render_frame_host);

  // PlatformVerificationFlow and the pref service requires to be run/accessed
  // on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new CdmDocumentServiceImpl(*render_frame_host, std::move(receiver));
}

CdmDocumentServiceImpl::CdmDocumentServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<media::mojom::CdmDocumentService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

CdmDocumentServiceImpl::~CdmDocumentServiceImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CdmDocumentServiceImpl::ChallengePlatform(
    const std::string& service_id,
    const std::string& challenge,
    ChallengePlatformCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/40499115). This should be commented out at the mojom
  // level so that it's only available for ChromeOS.

#if BUILDFLAG(IS_CHROMEOS)
  bool success = platform_verification::PerformBrowserChecks(
      render_frame_host().GetMainFrame());
  if (!success) {
    std::move(callback).Run(false, std::string(), std::string(), std::string());
    return;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service->IsAvailable<crosapi::mojom::ContentProtection>() &&
      lacros_service
              ->GetInterfaceVersion<crosapi::mojom::ContentProtection>() >=
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
      content::WebContents::FromRenderFrameHost(&render_frame_host()),
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

  // TODO(crbug.com/40499115). This should be commented out at the mojom
  // level so that it's only available if Storage Id is available.

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
  // Check that the request is for a supported version.
  if (version == kCurrentStorageIdVersion ||
      version == kRequestLatestStorageIdVersion) {
    ComputeStorageId(
        GetStorageIdSaltFromProfile(&render_frame_host()), origin(),
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

#if BUILDFLAG(IS_CHROMEOS)
void CdmDocumentServiceImpl::IsVerifiedAccessEnabled(
    IsVerifiedAccessEnabledCallback callback) {
  // If we are in guest/incognito mode, then verified access is effectively
  // disabled.
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  if (profile->IsOffTheRecord() || profile->IsGuestSession()) {
    std::move(callback).Run(false);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service->IsAvailable<crosapi::mojom::ContentProtection>() &&
      lacros_service
              ->GetInterfaceVersion<crosapi::mojom::ContentProtection>() >=
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
      ash::kAttestationForContentProtectionEnabled, &enabled_for_device);
  std::move(callback).Run(enabled_for_device);
#endif  // else BUILDFLAG(IS_CHROMEOS_LACROS)
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
void CdmDocumentServiceImpl::GetMediaFoundationCdmData(
    GetMediaFoundationCdmDataCallback callback) {
  const url::Origin cdm_origin = origin();
  if (cdm_origin.opaque()) {
    mojo::ReportBadMessage("EME use is not allowed on opaque origin");
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());

  PrefService* user_prefs = profile->GetPrefs();
  std::unique_ptr<CdmPrefData> pref_data =
      CdmPrefServiceHelper::GetCdmPrefData(user_prefs, cdm_origin);

  if (!pref_data) {
    std::move(callback).Run(nullptr);
    return;
  }

  // PostTask because the task is doing IO operation that can block.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetMediaFoundationCdmDataInternal, profile->GetPath(),
                     std::move(pref_data)),
      std::move(callback));
}

void CdmDocumentServiceImpl::SetCdmClientToken(
    const std::vector<uint8_t>& client_token) {
  const url::Origin cdm_origin = origin();
  if (cdm_origin.opaque()) {
    mojo::ReportBadMessage("EME use is not allowed on opaque origin");
    return;
  }

  PrefService* user_prefs =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext())
          ->GetPrefs();
  CdmPrefServiceHelper::SetCdmClientToken(user_prefs, cdm_origin, client_token);
}

void CdmDocumentServiceImpl::OnCdmEvent(media::CdmEvent event,
                                        uint32_t hresult) {
  DVLOG(1) << __func__ << ": event=" << static_cast<int>(event);

  auto* monitor = MediaFoundationServiceMonitor::GetInstance();

  // Hardware context reset after power or display change is expected.
  if (event == media::CdmEvent::kHardwareContextReset) {
    bool has_change = monitor->HasRecentPowerOrDisplayChange();
    base::UmaHistogramBoolean(
        "Media.EME.MediaFoundationService.HardwareContextReset", has_change);
    if (has_change) {
      DVLOG(2) << __func__
               << ": HardwareContextReset ignored after power/display change";
      return;
    }
  }

  // CdmDocumentServiceImpl is shared by all CDMs in the same RenderFrame.
  //
  // We choose to only handle each event type at most once because:
  // 1. A site could create many CDM instances, e.g. to prefetch licenses. This
  //    could cause multiple errors to be reported.
  // 2. The media::Renderer could be destroyed and then recreated as part of the
  //    suspend/resume process (e.g. paused for long time). This could cause
  //    multiple significant playback or hardware context reset without playback
  //    to be reported.
  // In all cases, our data could be skewed if we don't throttle them.
  //
  // A different event will still be reported. For example, if an error happens
  // after a significant playback both will be reported. This is fine since
  // MediaFoundationServiceMonitor calculates a score.
  if (auto [ignored, inserted] = reported_cdm_event_.insert(event); !inserted) {
    DVLOG(2) << __func__ << ": Repeated CdmEvent ignored";
    return;
  }

  auto site = render_frame_host().GetSiteInstance()->GetSiteURL();
  switch (event) {
    case media::CdmEvent::kSignificantPlayback:
      monitor->OnSignificantPlayback(site);
      break;
    case media::CdmEvent::kPlaybackError:
    case media::CdmEvent::kCdmError:
      monitor->OnPlaybackOrCdmError(site, static_cast<HRESULT>(hresult));
      break;
    case media::CdmEvent::kHardwareContextReset:
      monitor->OnUnexpectedHardwareContextReset(site);
      break;
  }
}

// This function goes over each folder located under the MediaFoundationCdm
// store root path and delete them as needed. A folder needs to be deleted for
// the following reason:
// - The origin id the folder is associated with is no longer present in the
// PrefService
// - The folder refers to an origin matched by `filter` AND the folder was last
// modified between `start` and `end`
void DeleteMediaFoundationCdmData(
    const base::FilePath& profile_path,
    const std::map<std::string, url::Origin> origin_id_mapping,
    base::Time start,
    base::Time end,
    const base::RepeatingCallback<bool(const GURL&)>& filter) {
  auto cdm_store_path_root = GetCdmStorePathRootForProfile(profile_path);

  // Enumerate all folder under `cdm_store_path_root` which should give a list
  // of folder whose names are origin ids. Each folder contains CDM data
  // associated with that origin id.
  //
  base::FileEnumerator directory_enumerator(cdm_store_path_root,
                                            /*recursive=*/false,
                                            base::FileEnumerator::DIRECTORIES);

  for (auto file_path = directory_enumerator.Next(); !file_path.value().empty();
       file_path = directory_enumerator.Next()) {
    // The folder name is a string representation of a base::UnguessableToken,
    // using MaybeAsASCII() is fine.
    std::string origin_id_string = file_path.BaseName().MaybeAsASCII();
    if (origin_id_string.empty())
      continue;

    DVLOG(2) << __func__ << ": Processing: " << file_path;
    std::optional<url::Origin> origin = std::nullopt;
    if (origin_id_mapping.count(origin_id_string) != 0)
      origin = origin_id_mapping.at(origin_id_string);

    // If we couldn't find the origin, this mean the origin was not present in
    // the PrefService and we should also delete the folder.
    if (!origin) {
      base::DeletePathRecursively(file_path);
      continue;
    }

    // Null filter indicates that we should delete everything.
    if (filter && !filter.Run(GURL(origin->Serialize())))
      continue;

    // Now go over every files under the current folder and delete them if
    // needed.
    base::FileEnumerator file_enumerator(file_path, /*recursive=*/true,
                                         base::FileEnumerator::FILES);

    // If at least one files was modified between `start` and `end`, we should
    // delete the whole folder.
    bool should_delete = false;
    for (auto cdm_data_file_path = file_enumerator.Next();
         !cdm_data_file_path.value().empty();
         cdm_data_file_path = file_enumerator.Next()) {
      DVLOG(2) << __func__ << ": - Processing: " << cdm_data_file_path;
      base::File::Info file_info;
      if (!base::GetFileInfo(cdm_data_file_path, &file_info)) {
        DVLOG(ERROR) << "Failed to get FileInfo";
        should_delete = true;
        break;
      }

      if (file_info.last_modified >= start &&
          (end.is_null() || file_info.last_modified <= end)) {
        DVLOG(2) << "Deleting file. Last modified: " << file_info.last_modified;
        should_delete = true;
        break;
      }
    }

    if (should_delete)
      base::DeletePathRecursively(file_path);
  }
}

void CdmDocumentServiceImpl::ClearCdmData(
    Profile* profile,
    base::Time start,
    base::Time end,
    const base::RepeatingCallback<bool(const GURL&)>& filter,
    base::OnceClosure complete_cb) {
  PrefService* user_prefs = profile->GetPrefs();
  CdmPrefServiceHelper::ClearCdmPreferenceData(user_prefs, start, end, filter);

  // Get the origin_id mapping here because the PrefService needs to be accessed
  // from the UI thread.
  auto origin_id_mapping = CdmPrefServiceHelper::GetOriginIdMapping(user_prefs);

  // PostTask because is doing IO operation that can block.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&DeleteMediaFoundationCdmData, profile->GetPath(),
                     std::move(origin_id_mapping), start, end, filter),
      std::move(complete_cb));
}
#endif  // BUILDFLAG(IS_WIN)
