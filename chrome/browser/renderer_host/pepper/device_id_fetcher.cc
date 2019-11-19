// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "crypto/encryptor.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "ppapi/c/pp_errors.h"
#include "rlz/buildflags/buildflags.h"

#if defined(OS_CHROMEOS)
#include "chromeos/cryptohome/system_salt_getter.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif

using content::BrowserPpapiHost;
using content::BrowserThread;
using content::RenderProcessHost;

namespace {

const char kDRMIdentifierFile[] = "Pepper DRM ID.0";

const uint32_t kSaltLength = 32;

void GetMachineIDAsync(
    const base::Callback<void(const std::string&)>& callback) {
#if defined(OS_WIN) && BUILDFLAG(ENABLE_RLZ)
  std::string result;
  rlz_lib::GetMachineId(&result);
  callback.Run(result);
#elif defined(OS_CHROMEOS)
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(callback);
#else
  // Not implemented for other platforms.
  NOTREACHED();
  callback.Run(std::string());
#endif
}

}  // namespace

DeviceIDFetcher::DeviceIDFetcher(int render_process_id)
    : in_progress_(false), render_process_id_(render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

DeviceIDFetcher::~DeviceIDFetcher() {}

bool DeviceIDFetcher::Start(const IDCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (in_progress_)
    return false;

  in_progress_ = true;
  callback_ = callback;

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&DeviceIDFetcher::CheckPrefsOnUIThread, this));
  return true;
}

// static
void DeviceIDFetcher::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kEnableDRM, true);
  prefs->RegisterStringPref(prefs::kDRMSalt, "");
}

// static
base::FilePath DeviceIDFetcher::GetLegacyDeviceIDPath(
    const base::FilePath& profile_path) {
  return profile_path.AppendASCII(kDRMIdentifierFile);
}

void DeviceIDFetcher::CheckPrefsOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = NULL;
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id_);
  if (render_process_host && render_process_host->GetBrowserContext()) {
    profile =
        Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  }

  if (!profile || profile->IsOffTheRecord() ||
      !profile->GetPrefs()->GetBoolean(prefs::kEnableDRM)) {
    RunCallbackOnIOThread(std::string(), PP_ERROR_NOACCESS);
    return;
  }

  // Check if the salt pref is set. If it isn't, set it.
  std::string salt = profile->GetPrefs()->GetString(prefs::kDRMSalt);
  if (salt.empty()) {
    uint8_t salt_bytes[kSaltLength];
    crypto::RandBytes(salt_bytes, base::size(salt_bytes));
    // Since it will be stored in a string pref, convert it to hex.
    salt = base::HexEncode(salt_bytes, base::size(salt_bytes));
    profile->GetPrefs()->SetString(prefs::kDRMSalt, salt);
  }

#if defined(OS_CHROMEOS)
  // Try the legacy path first for ChromeOS. We pass the new salt in as well
  // in case the legacy id doesn't exist.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeviceIDFetcher::LegacyComputeAsync, this,
                     profile->GetPath(), salt));
#else
  // Get the machine ID and call ComputeOnUIThread with salt + machine_id.
  GetMachineIDAsync(
      base::Bind(&DeviceIDFetcher::ComputeOnUIThread, this, salt));
#endif
}

void DeviceIDFetcher::ComputeOnUIThread(const std::string& salt,
                                        const std::string& machine_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (machine_id.empty()) {
    LOG(ERROR) << "Empty machine id";
    RunCallbackOnIOThread(std::string(), PP_ERROR_FAILED);
    return;
  }

  // Build the identifier as follows:
  // SHA256(machine-id||service||SHA256(machine-id||service||salt))
  std::vector<uint8_t> salt_bytes;
  if (!base::HexStringToBytes(salt, &salt_bytes))
    salt_bytes.clear();
  if (salt_bytes.size() != kSaltLength) {
    LOG(ERROR) << "Unexpected salt bytes length: " << salt_bytes.size();
    RunCallbackOnIOThread(std::string(), PP_ERROR_FAILED);
    return;
  }

  char id_buf[256 / 8];  // 256-bits for SHA256
  std::string input = machine_id;
  input.append(kDRMIdentifierFile);
  input.append(salt_bytes.begin(), salt_bytes.end());
  crypto::SHA256HashString(input, &id_buf, sizeof(id_buf));
  std::string id = base::ToLowerASCII(
      base::HexEncode(reinterpret_cast<const void*>(id_buf), sizeof(id_buf)));
  input = machine_id;
  input.append(kDRMIdentifierFile);
  input.append(id);
  crypto::SHA256HashString(input, &id_buf, sizeof(id_buf));
  id = base::ToLowerASCII(
      base::HexEncode(reinterpret_cast<const void*>(id_buf), sizeof(id_buf)));

  RunCallbackOnIOThread(id, PP_OK);
}

// TODO(raymes): This is temporary code to migrate ChromeOS devices to the new
// scheme for generating device IDs. Delete this once we are sure most ChromeOS
// devices have been migrated.
void DeviceIDFetcher::LegacyComputeAsync(const base::FilePath& profile_path,
                                         const std::string& salt) {
  std::string id;
  // First check if the legacy device ID file exists on ChromeOS. If it does, we
  // should just return that.
  base::FilePath id_path = GetLegacyDeviceIDPath(profile_path);
  if (base::PathExists(id_path)) {
    if (base::ReadFileToString(id_path, &id) && !id.empty()) {
      RunCallbackOnIOThread(id, PP_OK);
      return;
    }
  }
  // If we didn't find an ID, get the machine ID and call the new code path to
  // generate an ID.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&GetMachineIDAsync,
                                base::Bind(&DeviceIDFetcher::ComputeOnUIThread,
                                           this, salt)));
}

void DeviceIDFetcher::RunCallbackOnIOThread(const std::string& id,
                                            int32_t result) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&DeviceIDFetcher::RunCallbackOnIOThread, this,
                                  id, result));
    return;
  }
  in_progress_ = false;
  callback_.Run(id, result);
}
