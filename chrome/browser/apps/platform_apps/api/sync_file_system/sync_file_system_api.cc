// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/extension_sync_event_observer.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_file_status.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/common/apps/platform_apps/api/sync_file_system.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserContext;
using content::BrowserThread;
using ::sync_file_system::ConflictResolutionPolicy;
using ::sync_file_system::SyncFileStatus;
using ::sync_file_system::SyncFileSystemServiceFactory;
using ::sync_file_system::SyncStatusCode;

namespace chrome_apps {
namespace api {

namespace {

// Error messages.
const char kErrorMessage[] = "%s (error code: %d).";
const char kUnsupportedConflictResolutionPolicy[] =
    "Policy %s is not supported.";

::sync_file_system::SyncFileSystemService* GetSyncFileSystemService(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ::sync_file_system::SyncFileSystemService* service =
      SyncFileSystemServiceFactory::GetForProfile(profile);
  if (!service)
    return nullptr;
  ExtensionSyncEventObserver* observer =
      ExtensionSyncEventObserver::GetFactoryInstance()->Get(profile);
  if (!observer)
    return nullptr;
  observer->InitializeForService(service);
  return service;
}

std::string ErrorToString(SyncStatusCode code) {
  return base::StringPrintf(kErrorMessage,
                            ::sync_file_system::SyncStatusCodeToString(code),
                            static_cast<int>(code));
}

const char* QuotaStatusCodeToString(blink::mojom::QuotaStatusCode status) {
  switch (status) {
    case blink::mojom::QuotaStatusCode::kOk:
      return "OK.";
    case blink::mojom::QuotaStatusCode::kErrorNotSupported:
      return "Operation not supported.";
    case blink::mojom::QuotaStatusCode::kErrorInvalidModification:
      return "Invalid modification.";
    case blink::mojom::QuotaStatusCode::kErrorInvalidAccess:
      return "Invalid access.";
    case blink::mojom::QuotaStatusCode::kErrorAbort:
      return "Quota operation aborted.";
    case blink::mojom::QuotaStatusCode::kUnknown:
      return "Unknown error.";
  }
  NOTREACHED_IN_MIGRATION();
  return "Unknown error.";
}

}  // namespace

ExtensionFunction::ResponseAction
SyncFileSystemDeleteFileSystemFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& url = args()[0].GetString();

  scoped_refptr<storage::FileSystemContext> file_system_context =
      browser_context()
          ->GetStoragePartition(render_frame_host()->GetSiteInstance())
          ->GetFileSystemContext();
  storage::FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(url)));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      BindOnce(
          &storage::FileSystemContext::DeleteFileSystem, file_system_context,
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(source_url())),
          file_system_url.type(),
          BindOnce(&SyncFileSystemDeleteFileSystemFunction::DidDeleteFileSystem,
                   this)));
  return RespondLater();
}

void SyncFileSystemDeleteFileSystemFunction::DidDeleteFileSystem(
    base::File::Error error) {
  // Repost to switch from IO thread to UI thread for SendResponse().
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        BindOnce(&SyncFileSystemDeleteFileSystemFunction::DidDeleteFileSystem,
                 this, error));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error != base::File::FILE_OK) {
    base::Value::List error_result;
    error_result.Append(false);
    Respond(ErrorWithArguments(
        std::move(error_result),
        ErrorToString(::sync_file_system::FileErrorToSyncStatusCode(error))));
    return;
  }

  Respond(WithArguments(true));
}

ExtensionFunction::ResponseAction
SyncFileSystemRequestFileSystemFunction::Run() {
  // SyncFileSystem initialization is done in OpenFileSystem below, but we call
  // GetSyncFileSystemService here too to initialize sync event observer for
  // extensions API.
  if (!GetSyncFileSystemService(browser_context()))
    return RespondNow(Error(""));

  // Initializes sync context for this extension and continue to open
  // a new file system.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      BindOnce(&storage::FileSystemContext::OpenFileSystem,
               GetFileSystemContext(),
               blink::StorageKey::CreateFirstParty(
                   url::Origin::Create(source_url())),
               /*bucket=*/std::nullopt, storage::kFileSystemTypeSyncable,
               storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
               base::BindOnce(&self::DidOpenFileSystem, this)));
  return RespondLater();
}

storage::FileSystemContext*
SyncFileSystemRequestFileSystemFunction::GetFileSystemContext() {
  DCHECK(render_frame_host());
  return browser_context()
      ->GetStoragePartition(render_frame_host()->GetSiteInstance())
      ->GetFileSystemContext();
}

void SyncFileSystemRequestFileSystemFunction::DidOpenFileSystem(
    const storage::FileSystemURL& root_url,
    const std::string& file_system_name,
    base::File::Error error) {
  // Repost to switch from IO thread to UI thread for SendResponse().
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        BindOnce(&SyncFileSystemRequestFileSystemFunction::DidOpenFileSystem,
                 this, root_url, file_system_name, error));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error != base::File::FILE_OK) {
    Respond(Error(
        ErrorToString(::sync_file_system::FileErrorToSyncStatusCode(error))));
    return;
  }

  base::Value::Dict dict;
  dict.Set("name", file_system_name);
  dict.Set("root", ::sync_file_system::GetSyncableFileSystemRootURI(
                       root_url.origin().GetURL())
                       .spec());
  Respond(WithArguments(std::move(dict)));
}

ExtensionFunction::ResponseAction SyncFileSystemGetFileStatusFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& url = args()[0].GetString();

  scoped_refptr<storage::FileSystemContext> file_system_context =
      browser_context()
          ->GetStoragePartition(render_frame_host()->GetSiteInstance())
          ->GetFileSystemContext();
  storage::FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(url)));

  ::sync_file_system::SyncFileSystemService* sync_file_system_service =
      GetSyncFileSystemService(browser_context());
  if (!sync_file_system_service)
    return RespondNow(Error(""));

  sync_file_system_service->GetFileSyncStatus(
      file_system_url,
      BindOnce(&SyncFileSystemGetFileStatusFunction::DidGetFileStatus, this));
  return RespondLater();
}

void SyncFileSystemGetFileStatusFunction::DidGetFileStatus(
    const SyncStatusCode sync_status_code,
    const SyncFileStatus sync_file_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (sync_status_code != ::sync_file_system::SYNC_STATUS_OK) {
    Respond(Error(ErrorToString(sync_status_code)));
    return;
  }

  // Convert from C++ to JavaScript enum.
  Respond(ArgumentList(sync_file_system::GetFileStatus::Results::Create(
      SyncFileStatusToExtensionEnum(sync_file_status))));
}

SyncFileSystemGetFileStatusesFunction::SyncFileSystemGetFileStatusesFunction() {
}

SyncFileSystemGetFileStatusesFunction::
    ~SyncFileSystemGetFileStatusesFunction() {}

ExtensionFunction::ResponseAction SyncFileSystemGetFileStatusesFunction::Run() {
  // All FileEntries converted into array of URL Strings in JS custom bindings.
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_list());
  const base::Value::List& file_entry_urls = args()[0].GetList();

  scoped_refptr<storage::FileSystemContext> file_system_context =
      browser_context()
          ->GetStoragePartition(render_frame_host()->GetSiteInstance())
          ->GetFileSystemContext();

  // Map each file path->SyncFileStatus in the callback map.
  // TODO(calvinlo): Overload GetFileSyncStatus to take in URL array.
  num_expected_results_ = file_entry_urls.size();
  num_results_received_ = 0;
  file_sync_statuses_.clear();
  ::sync_file_system::SyncFileSystemService* sync_file_system_service =
      GetSyncFileSystemService(browser_context());
  if (!sync_file_system_service)
    return RespondNow(Error(""));

  for (const auto& entry : file_entry_urls) {
    std::string url;
    if (entry.is_string())
      url = entry.GetString();
    storage::FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(GURL(url)));

    sync_file_system_service->GetFileSyncStatus(
        file_system_url,
        BindOnce(&SyncFileSystemGetFileStatusesFunction::DidGetFileStatus, this,
                 file_system_url));
  }

  return RespondLater();
}

void SyncFileSystemGetFileStatusesFunction::DidGetFileStatus(
    const storage::FileSystemURL& file_system_url,
    SyncStatusCode sync_status_code,
    SyncFileStatus sync_file_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  num_results_received_++;
  DCHECK_LE(num_results_received_, num_expected_results_);

  file_sync_statuses_[file_system_url] =
      std::make_pair(sync_status_code, sync_file_status);

  // Keep mapping file statuses until all of them have been received.
  // TODO(calvinlo): Get rid of this check when batch version of
  // GetFileSyncStatus(GURL urls[]); is added.
  if (num_results_received_ < num_expected_results_)
    return;

  // All results received. Dump array of statuses into extension enum values.
  // Note that the enum types need to be set as strings manually as the
  // autogenerated Results::Create function thinks the enum values should be
  // returned as int values.
  base::Value::List status_array;
  for (auto it = file_sync_statuses_.begin(); it != file_sync_statuses_.end();
       ++it) {
    SyncStatusCode file_error = it->second.first;
    if (file_error == ::sync_file_system::SYNC_STATUS_OK)
      continue;

    base::Value::Dict dict;

    const storage::FileSystemURL& url = it->first;
    sync_file_system::FileStatus file_status =
        SyncFileStatusToExtensionEnum(it->second.second);

    dict.Set("entry", *CreateDictionaryValueForFileSystemEntry(
                          url, ::sync_file_system::SYNC_FILE_TYPE_FILE));
    dict.Set("status", ToString(file_status));

    dict.Set("error", ErrorToString(file_error));

    status_array.Append(std::move(dict));
  }
  Respond(WithArguments(std::move(status_array)));
}

ExtensionFunction::ResponseAction
SyncFileSystemGetUsageAndQuotaFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& url = args()[0].GetString();

  scoped_refptr<storage::FileSystemContext> file_system_context =
      browser_context()
          ->GetStoragePartition(render_frame_host()->GetSiteInstance())
          ->GetFileSystemContext();
  storage::FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(url)));

  scoped_refptr<storage::QuotaManager> quota_manager =
      browser_context()
          ->GetStoragePartition(render_frame_host()->GetSiteInstance())
          ->GetQuotaManager();

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      BindOnce(
          &storage::QuotaManager::GetUsageAndQuotaForWebApps, quota_manager,
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(source_url())),
          storage::FileSystemTypeToQuotaStorageType(file_system_url.type()),
          BindOnce(&SyncFileSystemGetUsageAndQuotaFunction::DidGetUsageAndQuota,
                   this)));

  return RespondLater();
}

void SyncFileSystemGetUsageAndQuotaFunction::DidGetUsageAndQuota(
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  // Repost to switch from IO thread to UI thread for SendResponse().
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        BindOnce(&SyncFileSystemGetUsageAndQuotaFunction::DidGetUsageAndQuota,
                 this, status, usage, quota));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::mojom::QuotaStatusCode::kOk) {
    Respond(Error(QuotaStatusCodeToString(status)));
    return;
  }

  sync_file_system::StorageInfo info;
  info.usage_bytes = usage;
  info.quota_bytes = quota;
  Respond(
      ArgumentList(sync_file_system::GetUsageAndQuota::Results::Create(info)));
}

ExtensionFunction::ResponseAction
SyncFileSystemSetConflictResolutionPolicyFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& policy_string = args()[0].GetString();
  ConflictResolutionPolicy policy = ExtensionEnumToConflictResolutionPolicy(
      sync_file_system::ParseConflictResolutionPolicy(policy_string));
  if (policy != ::sync_file_system::CONFLICT_RESOLUTION_POLICY_LAST_WRITE_WIN) {
    return RespondNow(Error(base::StringPrintf(
        kUnsupportedConflictResolutionPolicy, policy_string.c_str())));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SyncFileSystemGetConflictResolutionPolicyFunction::Run() {
  return RespondNow(WithArguments(sync_file_system::ToString(
      sync_file_system::ConflictResolutionPolicy::kLastWriteWin)));
}

ExtensionFunction::ResponseAction
SyncFileSystemGetServiceStatusFunction::Run() {
  ::sync_file_system::SyncFileSystemService* service =
      GetSyncFileSystemService(browser_context());
  if (!service)
    return RespondNow(Error(kUnknownErrorDoNotUse));
  return RespondNow(
      ArgumentList(sync_file_system::GetServiceStatus::Results::Create(
          SyncServiceStateToExtensionEnum(service->GetSyncServiceState()))));
}

}  // namespace api
}  // namespace chrome_apps
