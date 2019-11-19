// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pepper_flash_settings_manager.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/pepper_flash_settings_helper.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "url/gurl.h"

using content::BrowserThread;

class PepperFlashSettingsManager::Core
    : public IPC::Listener,
      public base::RefCountedThreadSafe<Core, BrowserThread::DeleteOnIOThread> {
 public:
  Core(base::WeakPtr<PepperFlashSettingsManager> manager,
       content::BrowserContext* browser_context);

  void Initialize();

  // Notifies the core that it has been detached. Afterwards, no method should
  // be called any more.
  void Detach();

  void DeauthorizeContentLicenses(uint32_t request_id);
  void GetPermissionSettings(
      uint32_t request_id,
      PP_Flash_BrowserOperations_SettingType setting_type);
  void SetDefaultPermission(uint32_t request_id,
                            PP_Flash_BrowserOperations_SettingType setting_type,
                            PP_Flash_BrowserOperations_Permission permission,
                            bool clear_site_specific);
  void SetSitePermission(uint32_t request_id,
                         PP_Flash_BrowserOperations_SettingType setting_type,
                         const ppapi::FlashSiteSettings& sites);
  void GetSitesWithData(uint32_t request_id);
  void ClearSiteData(uint32_t request_id,
                     const std::string& site,
                     uint64_t flags,
                     uint64_t max_age);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<Core>;

  enum RequestType {
    INVALID_REQUEST_TYPE = 0,
    DEAUTHORIZE_CONTENT_LICENSES,
    GET_PERMISSION_SETTINGS,
    SET_DEFAULT_PERMISSION,
    SET_SITE_PERMISSION,
    GET_SITES_WITH_DATA,
    CLEAR_SITE_DATA,
  };

  enum State {
    STATE_UNINITIALIZED = 0,
    STATE_INITIALIZED,
    STATE_ERROR,
    STATE_DETACHED,
  };

  struct PendingRequest {
    PendingRequest()
        : id(0),
          type(INVALID_REQUEST_TYPE),
          setting_type(PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_CAMERAMIC),
          permission(PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT),
          clear_site_specific(false),
          flags(0),
          max_age(0) {
    }

    uint32_t id;
    RequestType type;

    // Used by GET_PERMISSION_SETTINGS, SET_DEFAULT_PERMISSION and
    // SET_SITE_PERMISSION.
    PP_Flash_BrowserOperations_SettingType setting_type;

    // Used by SET_DEFAULT_PERMISSION.
    PP_Flash_BrowserOperations_Permission permission;
    bool clear_site_specific;

    // Used by SET_SITE_PERMISSION.
    ppapi::FlashSiteSettings sites;

    // Used by CLEAR_SITE_DATA
    std::string site;
    uint64_t flags;
    uint64_t max_age;
  };

  ~Core() override;

  void ConnectToChannel(bool success, const IPC::ChannelHandle& handle);

  void InitializeOnIOThread();
  void DeauthorizeContentLicensesOnIOThread(uint32_t request_id);
  void DeauthorizeContentLicensesAsync(uint32_t request_id,
                                       const base::FilePath& profile_path);
  void DeauthorizeContentLicensesInPlugin(uint32_t request_id, bool success);
  void GetPermissionSettingsOnIOThread(
      uint32_t request_id,
      PP_Flash_BrowserOperations_SettingType setting_type);
  void SetDefaultPermissionOnIOThread(
      uint32_t request_id,
      PP_Flash_BrowserOperations_SettingType setting_type,
      PP_Flash_BrowserOperations_Permission permission,
      bool clear_site_specific);
  void SetSitePermissionOnIOThread(
      uint32_t request_id,
      PP_Flash_BrowserOperations_SettingType setting_type,
      const ppapi::FlashSiteSettings& sites);
  void GetSitesWithDataOnIOThread(uint32_t request_id);
  void ClearSiteDataOnIOThread(uint32_t request_id,
                               const std::string& site,
                               uint64_t flags,
                               uint64_t max_age);
  void DetachOnIOThread();

  void NotifyErrorFromIOThread();

  void NotifyDeauthorizeContentLicensesCompleted(uint32_t request_id,
                                                 bool success);
  void NotifyGetPermissionSettingsCompleted(
      uint32_t request_id,
      bool success,
      PP_Flash_BrowserOperations_Permission default_permission,
      const ppapi::FlashSiteSettings& sites);
  void NotifySetDefaultPermissionCompleted(uint32_t request_id, bool success);
  void NotifySetSitePermissionCompleted(uint32_t request_id, bool success);
  void NotifyGetSitesWithDataCompleted(uint32_t request_id,
                                       const std::vector<std::string>& sites);
  void NotifyClearSiteDataCompleted(uint32_t request_id, bool success);

  void NotifyError(
      const std::vector<std::pair<uint32_t, RequestType>>& notifications);

  // Message handlers.
  void OnDeauthorizeContentLicensesResult(uint32_t request_id, bool success);
  void OnGetPermissionSettingsResult(
      uint32_t request_id,
      bool success,
      PP_Flash_BrowserOperations_Permission default_permission,
      const ppapi::FlashSiteSettings& sites);
  void OnSetDefaultPermissionResult(uint32_t request_id, bool success);
  void OnSetSitePermissionResult(uint32_t request_id, bool success);
  void OnGetSitesWithDataResult(uint32_t request_id,
                                const std::vector<std::string>& sites);
  void OnClearSiteDataResult(uint32_t request_id, bool success);

  // Used only on the UI thread.
  base::WeakPtr<PepperFlashSettingsManager> manager_;

  // Used only on the I/O thread.
  base::FilePath plugin_data_path_;

  // The channel is NULL until we have opened a connection to the broker
  // process. Used only on the I/O thread.
  std::unique_ptr<IPC::Channel> channel_;

  // Used only on the I/O thread.
  State state_;

  // Requests that need to be sent once the channel to the broker process is
  // established. Used only on the I/O thread.
  std::vector<PendingRequest> pending_requests_;
  // Requests that have been sent but haven't got replied. Used only on the
  // I/O thread.
  std::map<uint32_t, RequestType> pending_responses_;

  // Used only on the I/O thread.
  scoped_refptr<content::PepperFlashSettingsHelper> helper_;

  // Path for the current profile. Must be retrieved on the UI thread from the
  // browser context when we start so we can use it later on the I/O thread.
  base::FilePath browser_context_path_;

  scoped_refptr<PluginPrefs> plugin_prefs_;
};

PepperFlashSettingsManager::Core::Core(
    base::WeakPtr<PepperFlashSettingsManager> manager,
    content::BrowserContext* browser_context)
    : manager_(manager),
      state_(STATE_UNINITIALIZED),
      browser_context_path_(browser_context->GetPath()),
      plugin_prefs_(PluginPrefs::GetForProfile(
          Profile::FromBrowserContext(browser_context))) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PepperFlashSettingsManager::Core::~Core() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void PepperFlashSettingsManager::Core::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&Core::InitializeOnIOThread, this));
}

void PepperFlashSettingsManager::Core::Detach() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This call guarantees that one ref is retained until we get to the DETACHED
  // state. This is important. Otherwise, if the ref count drops to zero on the
  // UI thread (which posts a task to delete this object on the I/O thread)
  // while the I/O thread doesn't know about it, methods on the I/O thread might
  // increase the ref count again and cause double deletion.
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&Core::DetachOnIOThread, this));
}

void PepperFlashSettingsManager::Core::DeauthorizeContentLicenses(
    uint32_t request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&Core::DeauthorizeContentLicensesOnIOThread,
                                this, request_id));
}

void PepperFlashSettingsManager::Core::GetPermissionSettings(
    uint32_t request_id,
    PP_Flash_BrowserOperations_SettingType setting_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&Core::GetPermissionSettingsOnIOThread, this,
                                request_id, setting_type));
}

void PepperFlashSettingsManager::Core::SetDefaultPermission(
    uint32_t request_id,
    PP_Flash_BrowserOperations_SettingType setting_type,
    PP_Flash_BrowserOperations_Permission permission,
    bool clear_site_specific) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&Core::SetDefaultPermissionOnIOThread, this, request_id,
                     setting_type, permission, clear_site_specific));
}

void PepperFlashSettingsManager::Core::SetSitePermission(
    uint32_t request_id,
    PP_Flash_BrowserOperations_SettingType setting_type,
    const ppapi::FlashSiteSettings& sites) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&Core::SetSitePermissionOnIOThread, this,
                                request_id, setting_type, sites));
}

void PepperFlashSettingsManager::Core::GetSitesWithData(uint32_t request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&Core::GetSitesWithDataOnIOThread, this, request_id));
}

void PepperFlashSettingsManager::Core::ClearSiteData(uint32_t request_id,
                                                     const std::string& site,
                                                     uint64_t flags,
                                                     uint64_t max_age) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&Core::ClearSiteDataOnIOThread, this,
                                request_id, site, flags, max_age));
}

bool PepperFlashSettingsManager::Core::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(Core, message)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_DeauthorizeContentLicensesResult,
                        OnDeauthorizeContentLicensesResult)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_GetPermissionSettingsResult,
                        OnGetPermissionSettingsResult)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_SetDefaultPermissionResult,
                        OnSetDefaultPermissionResult)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_SetSitePermissionResult,
                        OnSetSitePermissionResult)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_GetSitesWithDataResult,
                        OnGetSitesWithDataResult)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ClearSiteDataResult,
                        OnClearSiteDataResult)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  return true;
}

void PepperFlashSettingsManager::Core::OnChannelError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_DETACHED)
    return;

  NotifyErrorFromIOThread();
}

void PepperFlashSettingsManager::Core::ConnectToChannel(
    bool success,
    const IPC::ChannelHandle& handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_DETACHED)
    return;

  DCHECK(state_ == STATE_UNINITIALIZED);
  DCHECK(!channel_.get());

  if (!success) {
    DLOG(ERROR) << "Couldn't open plugin channel";
    NotifyErrorFromIOThread();
    return;
  }

  channel_ = IPC::Channel::CreateClient(handle, this,
                                        base::ThreadTaskRunnerHandle::Get());
  if (!channel_->Connect()) {
    DLOG(ERROR) << "Couldn't connect to plugin";
    NotifyErrorFromIOThread();
    return;
  }

  state_ = STATE_INITIALIZED;

  std::vector<PendingRequest> temp_pending_requests;
  temp_pending_requests.swap(pending_requests_);
  for (auto iter = temp_pending_requests.begin();
       iter != temp_pending_requests.end(); ++iter) {
    switch (iter->type) {
      case INVALID_REQUEST_TYPE:
        NOTREACHED();
        break;
      case DEAUTHORIZE_CONTENT_LICENSES:
        DeauthorizeContentLicensesOnIOThread(iter->id);
        break;
      case GET_PERMISSION_SETTINGS:
        GetPermissionSettingsOnIOThread(iter->id, iter->setting_type);
        break;
      case SET_DEFAULT_PERMISSION:
        SetDefaultPermissionOnIOThread(
            iter->id, iter->setting_type, iter->permission,
            iter->clear_site_specific);
        break;
      case SET_SITE_PERMISSION:
        SetSitePermissionOnIOThread(iter->id, iter->setting_type, iter->sites);
        break;
      case GET_SITES_WITH_DATA:
        GetSitesWithDataOnIOThread(iter->id);
        break;
      case CLEAR_SITE_DATA:
        ClearSiteDataOnIOThread(iter->id, iter->site, iter->flags,
                                iter->max_age);
        break;
    }
  }
}

void PepperFlashSettingsManager::Core::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(STATE_UNINITIALIZED, state_);

  content::WebPluginInfo plugin_info;
  if (!PepperFlashSettingsManager::IsPepperFlashInUse(plugin_prefs_.get(),
                                                      &plugin_info)) {
    NotifyErrorFromIOThread();
    return;
  }

  base::FilePath profile_path =
      browser_context_path_.Append(content::kPepperDataDirname);
#if defined(OS_WIN)
  plugin_data_path_ = profile_path.Append(plugin_info.name);
#else
  plugin_data_path_ = profile_path.Append(base::UTF16ToUTF8(plugin_info.name));
#endif

  helper_ = content::PepperFlashSettingsHelper::Create();
  content::PepperFlashSettingsHelper::OpenChannelCallback callback =
      base::Bind(&Core::ConnectToChannel, this);
  helper_->OpenChannelToBroker(plugin_info.path, callback);
}

void PepperFlashSettingsManager::Core::DeauthorizeContentLicensesOnIOThread(
    uint32_t request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(STATE_DETACHED, state_);

  if (state_ == STATE_UNINITIALIZED) {
    PendingRequest request;
    request.id = request_id;
    request.type = DEAUTHORIZE_CONTENT_LICENSES;
    pending_requests_.push_back(request);
    return;
  }

  pending_responses_.insert(
      std::make_pair(request_id, DEAUTHORIZE_CONTENT_LICENSES));
  if (state_ == STATE_ERROR) {
    NotifyErrorFromIOThread();
    return;
  }

#if defined(OS_CHROMEOS)
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&Core::DeauthorizeContentLicensesAsync, this, request_id,
                     browser_context_path_));
#else
  DeauthorizeContentLicensesInPlugin(request_id, true);
#endif
}

// TODO(raymes): This is temporary code to migrate ChromeOS devices to the new
// scheme for generating device IDs. Delete this once we are sure most ChromeOS
// devices have been migrated.
void PepperFlashSettingsManager::Core::DeauthorizeContentLicensesAsync(
    uint32_t request_id,
    const base::FilePath& profile_path) {
  // ChromeOS used to store the device ID in a file but this is no longer used.
  // Wipe that file.
  const base::FilePath& device_id_path =
      DeviceIDFetcher::GetLegacyDeviceIDPath(profile_path);
  bool success = base::DeleteFile(device_id_path, false);

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&Core::DeauthorizeContentLicensesInPlugin, this,
                                request_id, success));
}

void PepperFlashSettingsManager::Core::DeauthorizeContentLicensesInPlugin(
    uint32_t request_id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!success) {
    NotifyErrorFromIOThread();
    return;
  }
  IPC::Message* msg =
      new PpapiMsg_DeauthorizeContentLicenses(request_id, plugin_data_path_);
  if (!channel_->Send(msg)) {
    DLOG(ERROR) << "Couldn't send DeauthorizeContentLicenses message";
    // A failure notification for the current request will be sent since
    // |pending_responses_| has been updated.
    NotifyErrorFromIOThread();
  }
}

void PepperFlashSettingsManager::Core::GetPermissionSettingsOnIOThread(
    uint32_t request_id,
    PP_Flash_BrowserOperations_SettingType setting_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(STATE_DETACHED, state_);

  if (state_ == STATE_UNINITIALIZED) {
    PendingRequest request;
    request.id = request_id;
    request.type = GET_PERMISSION_SETTINGS;
    request.setting_type = setting_type;
    pending_requests_.push_back(request);
    return;
  }

  pending_responses_.insert(
      std::make_pair(request_id, GET_PERMISSION_SETTINGS));
  if (state_ == STATE_ERROR) {
    NotifyErrorFromIOThread();
    return;
  }

  IPC::Message* msg = new PpapiMsg_GetPermissionSettings(
      request_id, plugin_data_path_, setting_type);
  if (!channel_->Send(msg)) {
    DLOG(ERROR) << "Couldn't send GetPermissionSettings message";
    // A failure notification for the current request will be sent since
    // |pending_responses_| has been updated.
    NotifyErrorFromIOThread();
  }
}

void PepperFlashSettingsManager::Core::SetDefaultPermissionOnIOThread(
    uint32_t request_id,
    PP_Flash_BrowserOperations_SettingType setting_type,
    PP_Flash_BrowserOperations_Permission permission,
    bool clear_site_specific) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(STATE_DETACHED, state_);

  if (state_ == STATE_UNINITIALIZED) {
    PendingRequest request;
    request.id = request_id;
    request.type = SET_DEFAULT_PERMISSION;
    request.setting_type = setting_type;
    request.permission = permission;
    request.clear_site_specific = clear_site_specific;
    pending_requests_.push_back(request);
    return;
  }

  pending_responses_.insert(std::make_pair(request_id, SET_DEFAULT_PERMISSION));
  if (state_ == STATE_ERROR) {
    NotifyErrorFromIOThread();
    return;
  }

  IPC::Message* msg = new PpapiMsg_SetDefaultPermission(
      request_id, plugin_data_path_, setting_type, permission,
      clear_site_specific);
  if (!channel_->Send(msg)) {
    DLOG(ERROR) << "Couldn't send SetDefaultPermission message";
    // A failure notification for the current request will be sent since
    // |pending_responses_| has been updated.
    NotifyErrorFromIOThread();
  }
}

void PepperFlashSettingsManager::Core::SetSitePermissionOnIOThread(
    uint32_t request_id,
    PP_Flash_BrowserOperations_SettingType setting_type,
    const ppapi::FlashSiteSettings& sites) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(STATE_DETACHED, state_);

  if (state_ == STATE_UNINITIALIZED) {
    pending_requests_.push_back(PendingRequest());
    PendingRequest& request = pending_requests_.back();
    request.id = request_id;
    request.type = SET_SITE_PERMISSION;
    request.setting_type = setting_type;
    request.sites = sites;
    return;
  }

  pending_responses_.insert(std::make_pair(request_id, SET_SITE_PERMISSION));
  if (state_ == STATE_ERROR) {
    NotifyErrorFromIOThread();
    return;
  }

  IPC::Message* msg = new PpapiMsg_SetSitePermission(
      request_id, plugin_data_path_, setting_type, sites);
  if (!channel_->Send(msg)) {
    DLOG(ERROR) << "Couldn't send SetSitePermission message";
    // A failure notification for the current request will be sent since
    // |pending_responses_| has been updated.
    NotifyErrorFromIOThread();
  }
}

void PepperFlashSettingsManager::Core::GetSitesWithDataOnIOThread(
    uint32_t request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(STATE_DETACHED, state_);

  if (state_ == STATE_UNINITIALIZED) {
    pending_requests_.push_back(PendingRequest());
    PendingRequest& request = pending_requests_.back();
    request.id = request_id;
    request.type = GET_SITES_WITH_DATA;
    return;
  }

  pending_responses_.insert(std::make_pair(request_id, GET_SITES_WITH_DATA));
  if (state_ == STATE_ERROR) {
    NotifyErrorFromIOThread();
    return;
  }

  IPC::Message* msg = new PpapiMsg_GetSitesWithData(
      request_id, plugin_data_path_);
  if (!channel_->Send(msg)) {
    DLOG(ERROR) << "Couldn't send GetSitesWithData message";
    // A failure notification for the current request will be sent since
    // |pending_responses_| has been updated.
    NotifyErrorFromIOThread();
  }
}

void PepperFlashSettingsManager::Core::ClearSiteDataOnIOThread(
    uint32_t request_id,
    const std::string& site,
    uint64_t flags,
    uint64_t max_age) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(STATE_DETACHED, state_);

  if (state_ == STATE_UNINITIALIZED) {
    pending_requests_.push_back(PendingRequest());
    PendingRequest& request = pending_requests_.back();
    request.id = request_id;
    request.type = CLEAR_SITE_DATA;
    request.site = site;
    request.flags = flags;
    request.max_age = max_age;
    return;
  }

  pending_responses_.insert(std::make_pair(request_id, CLEAR_SITE_DATA));
  if (state_ == STATE_ERROR) {
    NotifyErrorFromIOThread();
    return;
  }

  IPC::Message* msg = new PpapiMsg_ClearSiteData(
      request_id, plugin_data_path_, site, flags, max_age);
  if (!channel_->Send(msg)) {
    DLOG(ERROR) << "Couldn't send ClearSiteData message";
    // A failure notification for the current request will be sent since
    // |pending_responses_| has been updated.
    NotifyErrorFromIOThread();
  }
}

void PepperFlashSettingsManager::Core::DetachOnIOThread() {
  state_ = STATE_DETACHED;
}

void PepperFlashSettingsManager::Core::NotifyErrorFromIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_DETACHED)
    return;

  state_ = STATE_ERROR;
  std::vector<std::pair<uint32_t, RequestType>> notifications;
  for (auto iter = pending_requests_.begin(); iter != pending_requests_.end();
       ++iter) {
    notifications.push_back(std::make_pair(iter->id, iter->type));
  }
  pending_requests_.clear();
  notifications.insert(notifications.end(), pending_responses_.begin(),
                       pending_responses_.end());
  pending_responses_.clear();

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&Core::NotifyError, this, notifications));
}

void PepperFlashSettingsManager::Core::
    NotifyDeauthorizeContentLicensesCompleted(uint32_t request_id,
                                              bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (manager_.get()) {
    manager_->client_->OnDeauthorizeFlashContentLicensesCompleted(request_id,
                                                                  success);
  }
}

void PepperFlashSettingsManager::Core::NotifyGetPermissionSettingsCompleted(
    uint32_t request_id,
    bool success,
    PP_Flash_BrowserOperations_Permission default_permission,
    const ppapi::FlashSiteSettings& sites) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (manager_.get()) {
    manager_->client_->OnGetPermissionSettingsCompleted(
        request_id, success, default_permission, sites);
  }
}

void PepperFlashSettingsManager::Core::NotifySetDefaultPermissionCompleted(
    uint32_t request_id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (manager_.get()) {
    manager_->client_->OnSetDefaultPermissionCompleted(
        request_id, success);
  }
}

void PepperFlashSettingsManager::Core::NotifySetSitePermissionCompleted(
    uint32_t request_id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (manager_.get()) {
    manager_->client_->OnSetSitePermissionCompleted(
        request_id, success);
  }
}

void PepperFlashSettingsManager::Core::NotifyGetSitesWithDataCompleted(
    uint32_t request_id,
    const std::vector<std::string>& sites) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (manager_.get()) {
    manager_->client_->OnGetSitesWithDataCompleted(
        request_id, sites);
  }
}

void PepperFlashSettingsManager::Core::NotifyClearSiteDataCompleted(
    uint32_t request_id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (manager_.get())
    manager_->client_->OnClearSiteDataCompleted(request_id, success);
}

void PepperFlashSettingsManager::Core::NotifyError(
    const std::vector<std::pair<uint32_t, RequestType>>& notifications) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<Core> protector(this);
  for (auto iter = notifications.begin(); iter != notifications.end(); ++iter) {
    // Check |manager_| for each iteration in case it is destroyed in one of
    // the callbacks.
    if (!manager_.get())
      return;

    switch (iter->second) {
      case INVALID_REQUEST_TYPE:
        NOTREACHED();
        break;
      case DEAUTHORIZE_CONTENT_LICENSES:
        manager_->client_->OnDeauthorizeFlashContentLicensesCompleted(
            iter->first, false);
        break;
      case GET_PERMISSION_SETTINGS:
        manager_->client_->OnGetPermissionSettingsCompleted(
            iter->first, false, PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT,
            ppapi::FlashSiteSettings());
        break;
      case SET_DEFAULT_PERMISSION:
        manager_->client_->OnSetDefaultPermissionCompleted(
            iter->first, false);
        break;
      case SET_SITE_PERMISSION:
        manager_->client_->OnSetSitePermissionCompleted(iter->first, false);
        break;
      case GET_SITES_WITH_DATA:
        manager_->client_->OnGetSitesWithDataCompleted(
            iter->first, std::vector<std::string>());
        break;
      case CLEAR_SITE_DATA:
        manager_->client_->OnClearSiteDataCompleted(iter->first, false);
        break;
    }
  }

  if (manager_.get())
    manager_->OnError(this);
}

void PepperFlashSettingsManager::Core::OnDeauthorizeContentLicensesResult(
    uint32_t request_id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_DETACHED)
    return;

  DLOG_IF(ERROR, !success) << "DeauthorizeContentLicenses returned error";

  auto iter = pending_responses_.find(request_id);
  if (iter == pending_responses_.end())
    return;

  DCHECK_EQ(iter->second, DEAUTHORIZE_CONTENT_LICENSES);

  pending_responses_.erase(iter);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&Core::NotifyDeauthorizeContentLicensesCompleted, this,
                     request_id, success));
}

void PepperFlashSettingsManager::Core::OnGetPermissionSettingsResult(
    uint32_t request_id,
    bool success,
    PP_Flash_BrowserOperations_Permission default_permission,
    const ppapi::FlashSiteSettings& sites) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_DETACHED)
    return;

  DLOG_IF(ERROR, !success) << "GetPermissionSettings returned error";

  auto iter = pending_responses_.find(request_id);
  if (iter == pending_responses_.end())
    return;

  DCHECK_EQ(iter->second, GET_PERMISSION_SETTINGS);

  pending_responses_.erase(iter);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&Core::NotifyGetPermissionSettingsCompleted, this,
                     request_id, success, default_permission, sites));
}

void PepperFlashSettingsManager::Core::OnSetDefaultPermissionResult(
    uint32_t request_id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_DETACHED)
    return;

  DLOG_IF(ERROR, !success) << "SetDefaultPermission returned error";

  auto iter = pending_responses_.find(request_id);
  if (iter == pending_responses_.end())
    return;

  DCHECK_EQ(iter->second, SET_DEFAULT_PERMISSION);

  pending_responses_.erase(iter);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&Core::NotifySetDefaultPermissionCompleted,
                                this, request_id, success));
}

void PepperFlashSettingsManager::Core::OnSetSitePermissionResult(
    uint32_t request_id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_DETACHED)
    return;

  DLOG_IF(ERROR, !success) << "SetSitePermission returned error";

  auto iter = pending_responses_.find(request_id);
  if (iter == pending_responses_.end())
    return;

  DCHECK_EQ(iter->second, SET_SITE_PERMISSION);

  pending_responses_.erase(iter);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&Core::NotifySetSitePermissionCompleted, this,
                                request_id, success));
}

void PepperFlashSettingsManager::Core::OnGetSitesWithDataResult(
    uint32_t request_id,
    const std::vector<std::string>& sites) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_DETACHED)
    return;

  auto iter = pending_responses_.find(request_id);
  if (iter == pending_responses_.end())
    return;

  DCHECK_EQ(iter->second, GET_SITES_WITH_DATA);

  pending_responses_.erase(iter);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&Core::NotifyGetSitesWithDataCompleted, this,
                                request_id, sites));
}

void PepperFlashSettingsManager::Core::OnClearSiteDataResult(
    uint32_t request_id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_DETACHED)
    return;

  DLOG_IF(ERROR, !success) << "ClearSiteData returned error";

  auto iter = pending_responses_.find(request_id);
  if (iter == pending_responses_.end())
    return;

  DCHECK_EQ(iter->second, CLEAR_SITE_DATA);

  pending_responses_.erase(iter);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&Core::NotifyClearSiteDataCompleted, this,
                                request_id, success));
}

PepperFlashSettingsManager::PepperFlashSettingsManager(
    Client* client,
    content::BrowserContext* browser_context)
    : client_(client), browser_context_(browser_context), next_request_id_(1) {
  DCHECK(client);
  DCHECK(browser_context);
}

PepperFlashSettingsManager::~PepperFlashSettingsManager() {
  if (core_.get())
    core_->Detach();
}

// static
bool PepperFlashSettingsManager::IsPepperFlashInUse(
    PluginPrefs* plugin_prefs,
    content::WebPluginInfo* plugin_info) {
  if (!plugin_prefs)
    return false;

  content::PluginService* plugin_service =
      content::PluginService::GetInstance();
  std::vector<content::WebPluginInfo> plugins;
  plugin_service->GetPluginInfoArray(
      GURL(), content::kFlashPluginSwfMimeType, false, &plugins, NULL);

  for (auto iter = plugins.begin(); iter != plugins.end(); ++iter) {
    if (iter->is_pepper_plugin() && plugin_prefs->IsPluginEnabled(*iter)) {
      if (plugin_info)
        *plugin_info = *iter;
      return true;
    }
  }
  return false;
}

// static
void PepperFlashSettingsManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kPepperFlashSettingsEnabled, true);
}

uint32_t PepperFlashSettingsManager::DeauthorizeContentLicenses(
    PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Clear the device ID salt which has the effect of regenerating a device
  // ID. Since this happens synchronously (and on the UI thread), we don't have
  // to add it to a pending request.
  prefs->ClearPref(prefs::kDRMSalt);

  EnsureCoreExists();
  uint32_t id = GetNextRequestId();
  core_->DeauthorizeContentLicenses(id);
  return id;
}

uint32_t PepperFlashSettingsManager::GetPermissionSettings(
    PP_Flash_BrowserOperations_SettingType setting_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  EnsureCoreExists();
  uint32_t id = GetNextRequestId();
  core_->GetPermissionSettings(id, setting_type);
  return id;
}

uint32_t PepperFlashSettingsManager::SetDefaultPermission(
    PP_Flash_BrowserOperations_SettingType setting_type,
    PP_Flash_BrowserOperations_Permission permission,
    bool clear_site_specific) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  EnsureCoreExists();
  uint32_t id = GetNextRequestId();
  core_->SetDefaultPermission(id, setting_type, permission,
                              clear_site_specific);
  return id;
}

uint32_t PepperFlashSettingsManager::SetSitePermission(
    PP_Flash_BrowserOperations_SettingType setting_type,
    const ppapi::FlashSiteSettings& sites) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  EnsureCoreExists();
  uint32_t id = GetNextRequestId();
  core_->SetSitePermission(id, setting_type, sites);
  return id;
}

uint32_t PepperFlashSettingsManager::GetSitesWithData() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  EnsureCoreExists();
  uint32_t id = GetNextRequestId();
  core_->GetSitesWithData(id);
  return id;
}

uint32_t PepperFlashSettingsManager::ClearSiteData(const std::string& site,
                                                   uint64_t flags,
                                                   uint64_t max_age) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  EnsureCoreExists();
  uint32_t id = GetNextRequestId();
  core_->ClearSiteData(id, site, flags, max_age);
  return id;
}

uint32_t PepperFlashSettingsManager::GetNextRequestId() {
  return next_request_id_++;
}

void PepperFlashSettingsManager::EnsureCoreExists() {
  if (!core_.get()) {
    core_ = new Core(weak_ptr_factory_.GetWeakPtr(), browser_context_);
    core_->Initialize();
  }
}

void PepperFlashSettingsManager::OnError(Core* core) {
  DCHECK(core);
  if (core != core_.get())
    return;

  core_->Detach();
  core_.reset();
}
