// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_service.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background/background_contents_service_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::SiteInstance;
using content::WebContents;
using extensions::BackgroundInfo;
using extensions::Extension;
using extensions::UnloadedExtensionReason;

namespace {

const char kCrashedNotificationPrefix[] = "app.background.crashed.";
const char kNotifierId[] = "app.background.crashed";
bool g_disable_close_balloon_for_testing = false;

void CloseBalloon(const std::string& extension_id, Profile* profile) {
  if (g_disable_close_balloon_for_testing)
    return;

  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationHandler::Type::TRANSIENT,
      kCrashedNotificationPrefix + extension_id);
}

// Delegate for the app/extension crash notification balloon. Restarts the
// app/extension when the balloon is clicked.
class CrashNotificationDelegate : public message_center::NotificationDelegate {
 public:
  CrashNotificationDelegate(Profile* profile, const Extension* extension)
      : profile_(profile),
        is_hosted_app_(extension->is_hosted_app()),
        is_platform_app_(extension->is_platform_app()),
        extension_id_(extension->id()) {}

  CrashNotificationDelegate(const CrashNotificationDelegate&) = delete;
  CrashNotificationDelegate& operator=(const CrashNotificationDelegate&) =
      delete;

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    // Pass arguments by value as HandleClick() might destroy *this.
    HandleClick(is_hosted_app_, is_platform_app_, extension_id_, profile_);
    // *this might be destroyed now, do not access any members anymore!
  }

 private:
  ~CrashNotificationDelegate() override {}

  // Static to prevent accidental use of members as *this might get destroyed.
  static void HandleClick(bool is_hosted_app,
                          bool is_platform_app,
                          std::string extension_id,
                          Profile* profile) {
    // http://crbug.com/247790 involves a crash notification balloon being
    // clicked while the extension isn't in the TERMINATED state. In that case,
    // any of the "reload" methods called below can unload the extension, which
    // indirectly destroys the CrashNotificationDelegate, invalidating all its
    // member variables. Make sure to pass arguments by value when adding new
    // ones to this method.
    if (is_hosted_app) {
      // There can be a race here: user clicks the balloon, and simultaneously
      // reloads the sad tab for the app. So we check here to be safe before
      // loading the background page.
      BackgroundContentsService* service =
          BackgroundContentsServiceFactory::GetForProfile(profile);
      if (!service->GetAppBackgroundContents(extension_id))
        service->LoadBackgroundContentsForExtension(extension_id);
    } else if (is_platform_app) {
      apps::AppLoadService::Get(profile)->RestartApplication(extension_id);
    } else {
      extensions::ExtensionSystem::Get(profile)
          ->extension_service()
          ->ReloadExtension(extension_id);
    }

    CloseBalloon(extension_id, profile);
  }

  // This dangling raw_ptr occurred in:
  // unit_tests:
  // BackgroundContentsServiceNotificationTest.TestShowBalloonShutdown
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/1427454/test-results?q=ExactID%3Aninja%3A%2F%2Fchrome%2Ftest%3Aunit_tests%2FBackgroundContentsServiceNotificationTest.TestShowBalloonShutdown+VHash%3A728d3f3a440b40c1
  raw_ptr<Profile, FlakyDanglingUntriaged> profile_;
  bool is_hosted_app_;
  bool is_platform_app_;
  extensions::ExtensionId extension_id_;
};

void ReloadExtension(const std::string& extension_id, Profile* profile) {
  if (g_browser_process->IsShuttingDown() ||
      !g_browser_process->profile_manager()->IsValidProfile(profile)) {
    return;
  }

  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  if (!extension_system || !extension_system->extension_service() ||
      !extension_registry) {
    return;
  }

  if (!extension_registry->terminated_extensions().GetByID(extension_id)) {
    // Either the app/extension was uninstalled by policy or it has since
    // been restarted successfully by someone else (the user).
    return;
  }
  extension_system->extension_service()->ReloadExtension(extension_id);
}

}  // namespace

// Keys for the information we store about individual BackgroundContents in
// prefs. There is one top-level base::Value::Dict (stored at
// prefs::kRegisteredBackgroundContents). Information about each
// BackgroundContents is stored under that top-level base::Value::Dict, keyed
// by the parent application ID for easy lookup.
//
// kRegisteredBackgroundContents:
//    base::Value::Dict {
//       <appid_1>: { "url": <url1>, "name": <frame_name> },
//       <appid_2>: { "url": <url2>, "name": <frame_name> },
//         ... etc ...
//    }
const char kUrlKey[] = "url";
const char kFrameNameKey[] = "name";

// Defines the backoff policy used for attempting to reload extensions.
const net::BackoffEntry::Policy kExtensionReloadBackoffPolicy = {
    0,      // Initial errors to ignore before applying backoff.
    3000,   // Initial delay: 3 seconds.
    2,      // Multiply factor.
    0.1,    // Fuzzing percentage.
    -1,     // Maximum backoff time: -1 for no maximum.
    -1,     // Entry lifetime: -1 to never discard.
    false,  // Whether to always use initial delay. No-op as there are
            // no initial errors to ignore.
};

int BackgroundContentsService::restart_delay_in_ms_ = 3000;  // 3 seconds.

BackgroundContentsService::BackgroundContentsService(Profile* profile)
    : profile_(profile) {
  // Don't load/store preferences if the parent profile is incognito.
  if (!profile->IsOffTheRecord())
    prefs_ = profile->GetPrefs();

  // Listen for events to tell us when to load/unload persisted background
  // contents.
  StartObserving();
}

BackgroundContentsService::~BackgroundContentsService() {
  for (auto& observer : observers_)
    observer.OnBackgroundContentsServiceDestroying();
}

// static
void BackgroundContentsService::
    SetRestartDelayForForceInstalledAppsAndExtensionsForTesting(
        int restart_delay_in_ms) {
  restart_delay_in_ms_ = restart_delay_in_ms;
}

// static
std::string
BackgroundContentsService::GetNotificationDelegateIdForExtensionForTesting(
    const std::string& extension_id) {
  return kCrashedNotificationPrefix + extension_id;
}

// static
void BackgroundContentsService::DisableCloseBalloonForTesting(
    bool disable_close_balloon_for_testing) {
  g_disable_close_balloon_for_testing = disable_close_balloon_for_testing;
}

void BackgroundContentsService::ShowBalloonForTesting(
    const extensions::Extension* extension) {
  ShowBalloon(extension);
}

std::vector<BackgroundContents*>
BackgroundContentsService::GetBackgroundContents() const {
  std::vector<BackgroundContents*> contents;
  for (auto it = contents_map_.begin(); it != contents_map_.end(); ++it)
    contents.push_back(it->second.contents.get());
  return contents;
}

void BackgroundContentsService::StartObserving() {
  // On startup, load our background pages after extension-apps have loaded.
  extensions::ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE,
      base::BindOnce(&BackgroundContentsService::OnExtensionSystemReady,
                     weak_ptr_factory_.GetWeakPtr()));

  extension_host_registry_observation_.Observe(
      extensions::ExtensionHostRegistry::Get(profile_));

  // Listen for extension uninstall, load, unloaded notification.
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));
}

void BackgroundContentsService::OnExtensionSystemReady() {
  LoadBackgroundContentsFromManifests();
  LoadBackgroundContentsFromPrefs();
  SendChangeNotification();
}

void BackgroundContentsService::OnExtensionHostRenderProcessGone(
    content::BrowserContext* browser_context,
    extensions::ExtensionHost* extension_host) {
  // The notification may be for the on/off-the-record pair to this object's
  // `profile_`; since the BackgroundContentsService has its own instance in
  // incognito, only consider notifications from our exact context.
  if (browser_context != profile_)
    return;

  TRACE_EVENT0("browser,startup",
               "BackgroundContentsService::OnExtensionHostRenderProcessGone");
  HandleExtensionCrashed(extension_host->extension());
}

void BackgroundContentsService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (extension->is_hosted_app() &&
      BackgroundInfo::HasBackgroundPage(extension)) {
    // If there is a background page specified in the manifest for a hosted
    // app, then blow away registered urls in the pref.
    ShutdownAssociatedBackgroundContents(extension->id());

    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(profile);

    if (extension_system->is_ready()) {
      // Now load the manifest-specified background page. If service isn't
      // ready, then the background page will be loaded from the
      // EXTENSIONS_READY callback.
      LoadBackgroundContents(BackgroundInfo::GetBackgroundURL(extension),
                             "background", extension->id());
    }
  }

  // If there is an existing BackoffEntry for the extension, clear it if
  // the component extension stays loaded for 60 seconds. This avoids the
  // situation of effectively disabling an extension for the entire browser
  // session if there was a periodic crash (sometimes caused by another source).
  if (extensions::Manifest::IsComponentLocation(extension->location())) {
    ComponentExtensionBackoffEntryMap::const_iterator it =
        component_backoff_map_.find(extension->id());
    if (it != component_backoff_map_.end()) {
      net::BackoffEntry* entry = component_backoff_map_[extension->id()].get();
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&BackgroundContentsService::MaybeClearBackoffEntry,
                         weak_ptr_factory_.GetWeakPtr(), extension->id(),
                         entry->failure_count()),
          base::Seconds(60));
    }
  }

  // Close the crash notification balloon for the app/extension, if any.
  CloseBalloon(extension->id(), profile);
  SendChangeNotification();
}

void BackgroundContentsService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  switch (reason) {
    case UnloadedExtensionReason::DISABLE:                // Fall through.
    case UnloadedExtensionReason::TERMINATE:              // Fall through.
    case UnloadedExtensionReason::UNINSTALL:              // Fall through.
    case UnloadedExtensionReason::BLOCKLIST:              // Fall through.
    case UnloadedExtensionReason::LOCK_ALL:               // Fall through.
    case UnloadedExtensionReason::MIGRATED_TO_COMPONENT:  // Fall through.
    case UnloadedExtensionReason::PROFILE_SHUTDOWN:
      ShutdownAssociatedBackgroundContents(extension->id());
      SendChangeNotification();
      return;
    case UnloadedExtensionReason::UPDATE: {
      // If there is a manifest specified background page, then shut it down
      // here, since if the updated extension still has the background page,
      // then it will be loaded from LOADED callback. Otherwise, leave
      // BackgroundContents in place.
      // We don't call SendChangeNotification here - it will be generated
      // from the LOADED callback.
      if (BackgroundInfo::HasBackgroundPage(extension))
        ShutdownAssociatedBackgroundContents(extension->id());
      return;
      case UnloadedExtensionReason::UNDEFINED:
        // Fall through to undefined case.
        break;
    }
  }
  NOTREACHED_IN_MIGRATION() << "Undefined UnloadedExtensionReason.";
  return ShutdownAssociatedBackgroundContents(extension->id());
}

void BackgroundContentsService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // Make sure the extension-crash balloons are removed when the extension is
  // uninstalled/reloaded. We cannot do this from UNLOADED since a crashed
  // extension is unloaded immediately after the crash, not when user reloads or
  // uninstalls the extension.
  CloseBalloon(extension->id(), profile);
}

void BackgroundContentsService::RestartForceInstalledExtensionOnCrash(
    const Extension* extension) {
  int restart_delay = restart_delay_in_ms_;

  // If the extension was a component extension, use exponential backoff when
  // attempting to reload.
  if (extensions::Manifest::IsComponentLocation(extension->location())) {
    ComponentExtensionBackoffEntryMap::const_iterator it =
        component_backoff_map_.find(extension->id());

    // Create a BackoffEntry if this is the first time we try to reload this
    // particular extension.
    if (it == component_backoff_map_.end()) {
      std::unique_ptr<net::BackoffEntry> backoff_entry(
          new net::BackoffEntry(&kExtensionReloadBackoffPolicy));
      component_backoff_map_.insert(
          std::pair<extensions::ExtensionId,
                    std::unique_ptr<net::BackoffEntry>>(
              extension->id(), std::move(backoff_entry)));
    }

    net::BackoffEntry* entry = component_backoff_map_[extension->id()].get();
    entry->InformOfRequest(false);
    restart_delay = entry->GetTimeUntilRelease().InMilliseconds();
  }

  // Ugly implementation detail: ExtensionService listens to the same
  // notification that can lead us here, and asynchronously marks the extension
  // as terminated (with an effective delay of 0) to allow other systems to
  // receive the notification. However, that means we need to ensure this task
  // gets ran *after* that, so that the extension is in the terminated set by
  // the time we try to reload it (even if this service gets the notification
  // first).
  //
  // TODO(devlin): This would be unnecessary if we listened to the
  // OnExtensionUnloaded() notification and checked the unload reason.
  DCHECK_GT(restart_delay, 0);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&ReloadExtension, extension->id(), profile_),
      base::Milliseconds(restart_delay));
}

// Loads all background contents whose urls have been stored in prefs.
void BackgroundContentsService::LoadBackgroundContentsFromPrefs() {
  if (!prefs_)
    return;
  const base::Value::Dict& contents =
      prefs_->GetDict(prefs::kRegisteredBackgroundContents);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile_);
  DCHECK(extension_registry);
  for (const auto [extension_id, _] : contents) {
    // Check to make sure that the parent extension is still enabled.
    const Extension* extension =
        extension_registry->enabled_extensions().GetByID(extension_id);
    if (!extension) {
      // Normally, we shouldn't reach here - it shouldn't be possible for an app
      // to become uninstalled without the associated BackgroundContents being
      // unregistered via the EXTENSIONS_UNLOADED notification. However, this
      // is possible if there's e.g. a crash before we could save our prefs or
      // the user deletes the extension files manually rather than uninstalling
      // it.
      LOG(ERROR) << "No extension found for BackgroundContents - id = "
                 << extension_id;
      // Don't cancel out of our loop, just ignore this BackgroundContents and
      // load the next one.
      continue;
    }
    LoadBackgroundContentsFromDictionary(extension_id, contents);
  }
}

void BackgroundContentsService::SendChangeNotification() {
  for (auto& observer : observers_)
    observer.OnBackgroundContentsServiceChanged();
}

void BackgroundContentsService::MaybeClearBackoffEntry(
    const std::string extension_id,
    int expected_failure_count) {
  ComponentExtensionBackoffEntryMap::const_iterator it =
      component_backoff_map_.find(extension_id);
  if (it == component_backoff_map_.end())
    return;

  net::BackoffEntry* entry = component_backoff_map_[extension_id].get();

  // Only remove the BackoffEntry if there has has been no failure for
  // |extension_id| since loading.
  if (entry->failure_count() == expected_failure_count)
    component_backoff_map_.erase(it);
}

void BackgroundContentsService::LoadBackgroundContentsForExtension(
    const std::string& extension_id) {
  // First look if the manifest specifies a background page.
  const Extension* extension = extensions::ExtensionRegistry::Get(profile_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  DCHECK(!extension || extension->is_hosted_app());
  if (extension && BackgroundInfo::HasBackgroundPage(extension)) {
    LoadBackgroundContents(BackgroundInfo::GetBackgroundURL(extension),
                           "background", extension->id());
    return;
  }

  // Now look in the prefs.
  if (!prefs_)
    return;
  const base::Value::Dict& contents =
      prefs_->GetDict(prefs::kRegisteredBackgroundContents);
  LoadBackgroundContentsFromDictionary(extension_id, contents);
}

void BackgroundContentsService::LoadBackgroundContentsFromDictionary(
    const std::string& extension_id,
    const base::Value::Dict& contents) {
  extensions::ExtensionService* extensions_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(extensions_service);

  const base::Value::Dict* dict = contents.FindDict(extension_id);
  if (!dict)
    return;

  const std::string* maybe_frame_name = dict->FindString(kUrlKey);
  const std::string* maybe_url = dict->FindString(kFrameNameKey);
  std::string frame_name = maybe_frame_name ? *maybe_frame_name : std::string();
  std::string url = maybe_url ? *maybe_url : std::string();

  LoadBackgroundContents(GURL(url), frame_name, extension_id);
}

void BackgroundContentsService::LoadBackgroundContentsFromManifests() {
  for (const scoped_refptr<const extensions::Extension>& extension :
       extensions::ExtensionRegistry::Get(profile_)->enabled_extensions()) {
    if (extension->is_hosted_app() &&
        BackgroundInfo::HasBackgroundPage(extension.get())) {
      LoadBackgroundContents(BackgroundInfo::GetBackgroundURL(extension.get()),
                             "background", extension->id());
    }
  }
}

void BackgroundContentsService::LoadBackgroundContents(
    const GURL& url,
    const std::string& frame_name,
    const std::string& application_id) {
  // We are depending on the fact that we will initialize before any user
  // actions or session restore can take place, so no BackgroundContents should
  // be running yet for the passed application_id.
  DCHECK(!GetAppBackgroundContents(application_id));
  DCHECK(!application_id.empty());
  DCHECK(url.is_valid());
  DVLOG(1) << "Loading background content url: " << url;

  BackgroundContents* contents = CreateBackgroundContents(
      SiteInstance::CreateForURL(profile_, url), nullptr, true, frame_name,
      application_id, content::StoragePartitionConfig::CreateDefault(profile_),
      nullptr);

  contents->CreateRendererSoon(url);
}

BackgroundContents* BackgroundContentsService::CreateBackgroundContents(
    scoped_refptr<SiteInstance> site,
    content::RenderFrameHost* opener,
    bool is_new_browsing_instance,
    const std::string& frame_name,
    const std::string& application_id,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace) {
  auto contents = std::make_unique<BackgroundContents>(
      std::move(site), opener, is_new_browsing_instance, this, partition_config,
      session_storage_namespace);
  BackgroundContents* contents_ptr = contents.get();
  AddBackgroundContents(std::move(contents), application_id, frame_name);

  // Register the BackgroundContents internally, then send out a notification
  // to external listeners.
  BackgroundContentsOpenedDetails details = {contents_ptr, raw_ref(frame_name),
                                             raw_ref(application_id)};
  for (auto& observer : observers_)
    observer.OnBackgroundContentsOpened(details);

  // A new background contents has been created - notify our listeners.
  SendChangeNotification();
  return contents_ptr;
}

void BackgroundContentsService::DeleteBackgroundContents(
    BackgroundContents* contents) {
  contents_map_.erase(GetParentApplicationId(contents));
  SendChangeNotification();
}

void BackgroundContentsService::RegisterBackgroundContents(
    BackgroundContents* background_contents) {
  DCHECK(IsTracked(background_contents));
  if (!prefs_)
    return;

  // We store the first URL we receive for a given application. If there's
  // already an entry for this application, no need to do anything.
  // TODO(atwilson): Verify that this is the desired behavior based on developer
  // feedback (http://crbug.com/47118).
  ScopedDictPrefUpdate update(prefs_, prefs::kRegisteredBackgroundContents);
  base::Value::Dict& pref = update.Get();
  const std::string& appid = GetParentApplicationId(background_contents);
  if (pref.FindDict(appid)) {
    return;
  }

  // No entry for this application yet, so add one.
  base::Value::Dict dict;
  dict.Set(kUrlKey, background_contents->GetURL().spec());
  dict.Set(kFrameNameKey, contents_map_[appid].frame_name);
  pref.Set(appid, std::move(dict));
}

bool BackgroundContentsService::HasRegisteredBackgroundContents(
    const std::string& app_id) {
  if (!prefs_)
    return false;
  const base::Value::Dict& contents =
      prefs_->GetDict(prefs::kRegisteredBackgroundContents);
  return contents.Find(app_id);
}

void BackgroundContentsService::UnregisterBackgroundContents(
    BackgroundContents* background_contents) {
  if (!prefs_)
    return;
  DCHECK(IsTracked(background_contents));
  const std::string& appid = GetParentApplicationId(background_contents);
  ScopedDictPrefUpdate update(prefs_, prefs::kRegisteredBackgroundContents);
  update->Remove(appid);
}

void BackgroundContentsService::ShutdownAssociatedBackgroundContents(
    const std::string& appid) {
  BackgroundContents* contents = GetAppBackgroundContents(appid);
  if (contents) {
    UnregisterBackgroundContents(contents);
    // Background contents destructor shuts down the renderer.
    DeleteBackgroundContents(contents);
  }
}

void BackgroundContentsService::AddBackgroundContents(
    std::unique_ptr<BackgroundContents> contents,
    const std::string& application_id,
    const std::string& frame_name) {
  // Add the passed object to our list.
  DCHECK(!application_id.empty());
  BackgroundContentsInfo& info = contents_map_[application_id];
  info.contents = std::move(contents);
  info.frame_name = frame_name;

  CloseBalloon(application_id, profile_);
}

// Used by test code and debug checks to verify whether a given
// BackgroundContents is being tracked by this instance.
bool BackgroundContentsService::IsTracked(
    BackgroundContents* background_contents) const {
  return !GetParentApplicationId(background_contents).empty();
}

void BackgroundContentsService::AddObserver(
    BackgroundContentsServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void BackgroundContentsService::RemoveObserver(
    BackgroundContentsServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

BackgroundContents* BackgroundContentsService::GetAppBackgroundContents(
    const std::string& application_id) {
  BackgroundContentsMap::const_iterator it = contents_map_.find(application_id);
  return (it != contents_map_.end()) ? it->second.contents.get() : nullptr;
}

const std::string& BackgroundContentsService::GetParentApplicationId(
    BackgroundContents* contents) const {
  for (auto it = contents_map_.begin(); it != contents_map_.end(); ++it) {
    if (contents == it->second.contents.get())
      return it->first;
  }
  return base::EmptyString();
}

void BackgroundContentsService::AddWebContents(
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool* was_blocked) {
  Browser* browser = chrome::FindLastActiveWithProfile(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  if (browser) {
    chrome::AddWebContents(browser, nullptr, std::move(new_contents),
                           target_url, disposition, window_features);
  }
}

void BackgroundContentsService::OnBackgroundContentsNavigated(
    BackgroundContents* contents) {
  DCHECK(IsTracked(contents));
  // Do not register in the pref if the extension has a manifest-specified
  // background page.
  const std::string& appid = GetParentApplicationId(contents);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile_);
  const Extension* extension =
      extension_registry->enabled_extensions().GetByID(appid);
  if (extension && BackgroundInfo::HasBackgroundPage(extension))
    return;
  RegisterBackgroundContents(contents);
}

void BackgroundContentsService::OnBackgroundContentsTerminated(
    BackgroundContents* contents) {
  HandleExtensionCrashed(extensions::ExtensionRegistry::Get(profile_)
                             ->enabled_extensions()
                             .GetByID(GetParentApplicationId(contents)));
  DeleteBackgroundContents(contents);
}

void BackgroundContentsService::OnBackgroundContentsClosed(
    BackgroundContents* contents) {
  DCHECK(IsTracked(contents));
  UnregisterBackgroundContents(contents);
  DeleteBackgroundContents(contents);
  for (auto& observer : observers_)
    observer.OnBackgroundContentsClosed();
}

void BackgroundContentsService::Shutdown() {
  contents_map_.clear();
}

void BackgroundContentsService::HandleExtensionCrashed(
    const extensions::Extension* extension) {
  // When the extensions crash, notify the user about it and restart the crashed
  // contents.
  if (!extension)
    return;

  const bool force_installed =
      extensions::Manifest::IsComponentLocation(extension->location()) ||
      extensions::Manifest::IsPolicyLocation(extension->location());
  if (!force_installed) {
    ShowBalloon(extension);
  } else {
    // Restart the extension.
    RestartForceInstalledExtensionOnCrash(extension);
  }
}

void BackgroundContentsService::NotificationImageReady(
    const std::string extension_name,
    const std::string extension_id,
    const std::u16string message,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    const gfx::Image& icon) {
  NotificationDisplayService* notification_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  CHECK(notification_service);

  if (g_browser_process->IsShuttingDown()) {
    return;
  }

  gfx::Image notification_icon(icon);
  if (notification_icon.IsEmpty()) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    notification_icon = rb.GetImageNamed(IDR_EXTENSION_DEFAULT_ICON);
  }

  // Origin URL must be different from the crashed extension to avoid the
  // conflict. NotificationSystemObserver will cancel all notifications from
  // the same origin when OnExtensionUnloaded() is called.
  std::string id = kCrashedNotificationPrefix + extension_id;
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, std::u16string(), message,
      ui::ImageModel::FromImage(notification_icon), std::u16string(),
      GURL("chrome://extension-crash"),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
          ash::NotificationCatalogName::kBackgroundCrash),
#else
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      {}, delegate);
  notification_service->Display(NotificationHandler::Type::TRANSIENT,
                                notification,
                                /*metadata=*/nullptr);
}

// Show a popup notification balloon with a crash message for a given app/
// extension.
void BackgroundContentsService::ShowBalloon(const Extension* extension) {
  const std::u16string message = l10n_util::GetStringFUTF16(
      extension->is_app() ? IDS_BACKGROUND_CRASHED_APP_BALLOON_MESSAGE
                          : IDS_BACKGROUND_CRASHED_EXTENSION_BALLOON_MESSAGE,
      base::UTF8ToUTF16(extension->name()));
  extension_misc::ExtensionIcons size(extension_misc::EXTENSION_ICON_LARGE);
  extensions::ExtensionResource resource =
      extensions::IconsInfo::GetIconResource(extension, size,
                                             ExtensionIconSet::Match::kSmaller);
  // We can't just load the image in the Observe method below because, despite
  // what this method is called, it may call the callback synchronously.
  // However, it's possible that the extension went away during the interim,
  // so we'll bind all the pertinent data here.
  extensions::ImageLoader::Get(profile_)->LoadImageAsync(
      extension, resource, gfx::Size(size, size),
      base::BindOnce(&BackgroundContentsService::NotificationImageReady,
                     weak_ptr_factory_.GetWeakPtr(), extension->name(),
                     extension->id(), message,
                     base::MakeRefCounted<CrashNotificationDelegate>(
                         profile_, extension)));
}

BackgroundContentsService::BackgroundContentsInfo::BackgroundContentsInfo() =
    default;
BackgroundContentsService::BackgroundContentsInfo::~BackgroundContentsInfo() =
    default;
