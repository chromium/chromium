// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/error_console/error_console_factory.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/warning_service_factory.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/permissions_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace extensions {

namespace developer = api::developer_private;

static base::LazyInstance<BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>>::
    DestructorAtExit g_developer_private_api_factory =
        LAZY_INSTANCE_INITIALIZER;

class DeveloperPrivateAPI::WebContentsTracker
    : public content::WebContentsObserver {
 public:
  WebContentsTracker(base::WeakPtr<DeveloperPrivateAPI> api,
                     content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents), api_(api) {}

  WebContentsTracker(const WebContentsTracker&) = delete;
  WebContentsTracker& operator=(const WebContentsTracker&) = delete;

 private:
  ~WebContentsTracker() override = default;

  void WebContentsDestroyed() override {
    if (api_)
      api_->web_contents_data_.erase(web_contents());
    delete this;
  }

  base::WeakPtr<DeveloperPrivateAPI> api_;
};

DeveloperPrivateAPI::WebContentsData::WebContentsData() = default;
DeveloperPrivateAPI::WebContentsData::~WebContentsData() = default;
DeveloperPrivateAPI::WebContentsData::WebContentsData(WebContentsData&& other) =
    default;

// static
BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>*
DeveloperPrivateAPI::GetFactoryInstance() {
  return g_developer_private_api_factory.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<
    DeveloperPrivateAPI>::DeclareFactoryDependencies() {
  // Keep this in sync with observers DeveloperPrivateEventRouterShared
  // implements.
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ErrorConsoleFactory::GetInstance());
  DependsOn(ProcessManagerFactory::GetInstance());
  DependsOn(WarningServiceFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(PermissionsManager::GetFactory());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(AppWindowRegistry::Factory::GetInstance());
  DependsOn(ExtensionManagementFactory::GetInstance());
  DependsOn(CommandService::GetFactoryInstance());
  DependsOn(ChromeExtensionSystemFactory::GetInstance());
  DependsOn(ToolbarActionsModelFactory::GetInstance());
  DependsOn(AccountExtensionTracker::GetFactory());
#endif  // !BUILDFLAG(IS_ANDROID)
}

// static
DeveloperPrivateAPI* DeveloperPrivateAPI::Get(
    content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

DeveloperPrivateAPI::DeveloperPrivateAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  RegisterNotifications();
}

DeveloperPrivateAPI::UnpackedRetryId DeveloperPrivateAPI::AddUnpackedPath(
    content::WebContents* web_contents,
    const base::FilePath& path) {
  DCHECK(web_contents);
  last_unpacked_directory_ = path;
  WebContentsData* data = GetOrCreateWebContentsData(web_contents);
  IdToPathMap& paths = data->allowed_unpacked_paths;
  auto existing =
      std::ranges::find(paths, path, &IdToPathMap::value_type::second);
  if (existing != paths.end())
    return existing->first;

  UnpackedRetryId id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  paths[id] = path;
  return id;
}

base::FilePath DeveloperPrivateAPI::GetUnpackedPath(
    content::WebContents* web_contents,
    const UnpackedRetryId& id) const {
  const WebContentsData* data = GetWebContentsData(web_contents);
  if (!data)
    return base::FilePath();
  const IdToPathMap& paths = data->allowed_unpacked_paths;
  auto path_iter = paths.find(id);
  if (path_iter == paths.end())
    return base::FilePath();
  return path_iter->second;
}

void DeveloperPrivateAPI::SetDraggedPath(content::WebContents* web_contents,
                                         const base::FilePath& dragged_path) {
  WebContentsData* data = GetOrCreateWebContentsData(web_contents);
  data->dragged_path = dragged_path;
}

base::FilePath DeveloperPrivateAPI::GetDraggedPath(
    content::WebContents* web_contents) const {
  const WebContentsData* data = GetWebContentsData(web_contents);
  return data ? data->dragged_path : base::FilePath();
}

void DeveloperPrivateAPI::RegisterNotifications() {
  EventRouter::Get(profile_)->RegisterObserver(
      this, developer::OnItemStateChanged::kEventName);
  EventRouter::Get(profile_)->RegisterObserver(
      this, developer::OnUserSiteSettingsChanged::kEventName);
}

const DeveloperPrivateAPI::WebContentsData*
DeveloperPrivateAPI::GetWebContentsData(
    content::WebContents* web_contents) const {
  auto iter = web_contents_data_.find(web_contents);
  return iter == web_contents_data_.end() ? nullptr : &iter->second;
}

DeveloperPrivateAPI::WebContentsData*
DeveloperPrivateAPI::GetOrCreateWebContentsData(
    content::WebContents* web_contents) {
  auto iter = web_contents_data_.find(web_contents);
  if (iter != web_contents_data_.end())
    return &iter->second;

  // This is the first we've added this WebContents. Track its lifetime so we
  // can clean up the paths when it is destroyed.
  // WebContentsTracker manages its own lifetime.
  new WebContentsTracker(weak_factory_.GetWeakPtr(), web_contents);
  return &web_contents_data_[web_contents];
}

DeveloperPrivateAPI::~DeveloperPrivateAPI() = default;

void DeveloperPrivateAPI::Shutdown() {}

void DeveloperPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  if (!developer_private_event_router_) {
    developer_private_event_router_ =
        std::make_unique<DeveloperPrivateEventRouter>(profile_);
  }

  developer_private_event_router_->AddExtensionId(details.extension_id);
}

void DeveloperPrivateAPI::OnListenerRemoved(
    const EventListenerInfo& details) {
  if (!EventRouter::Get(profile_)->HasEventListener(
          developer::OnItemStateChanged::kEventName) &&
      !EventRouter::Get(profile_)->HasEventListener(
          developer::OnUserSiteSettingsChanged::kEventName)) {
    developer_private_event_router_.reset(nullptr);
  } else {
    developer_private_event_router_->RemoveExtensionId(details.extension_id);
  }
}

}  // namespace extensions
