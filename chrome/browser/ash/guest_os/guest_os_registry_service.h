// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_external_protocol_handler.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_scale_factor.h"

class Profile;
class PrefService;

namespace base {
class Clock;
class Time;
}  // namespace base

namespace apps {
class SvgIconTranscoder;
}  // namespace apps

namespace vm_tools {
namespace apps {
class ApplicationList;
}  // namespace apps
}  // namespace vm_tools

namespace guest_os {

using IconContentCallback = base::OnceCallback<void(std::string)>;
using CanHandleUrlCallback = base::RepeatingCallback<bool(const GURL&)>;

// The GuestOsRegistryService  stores information about Desktop Entries (apps)
// in Crostini. We store this in prefs so that it is readily available even when
// the VM isn't running. The registrations here correspond to .desktop files,
// which are detailed in the spec:
// https://www.freedesktop.org/wiki/Specifications/desktop-entry-spec/

// This class deals with several types of IDs, including:
// 1) Desktop File IDs (desktop_file_id):
//    - As per the desktop entry spec.
// 2) Crostini App List Ids (app_id):
//    - Valid extensions ids for apps stored in the registry, derived from the
//    desktop file id, vm name, and container name.
//    - The Terminal is a special case, using kCrostiniTerminalId (see below).
// 3) Exo Window App Ids (window_app_id):
//    - Retrieved from exo::GetShellApplicationId()
//    - For Wayland apps, this is the surface class of the app
//    - For X apps, this is of the form org.chromium.termina.wmclass.foo when
//    WM_CLASS is set to foo, or otherwise some string prefixed by
//    "org.chromium.termina." when WM_CLASS is not set.
// 4) Shelf App Ids (shelf_app_id):
//    - Used in ash::ShelfID::app_id
//    - Either a Window App Id prefixed by "crostini:" or a Crostini App Id.
//    - For pinned apps, this is a Crostini App Id.

// The default Terminal app does not correspond to a desktop file, but users
// of the registry can treat it as a regular app that is always installed.
// Internal to the registry, the pref entry only contains the last launch time
// so some care is required.
class GuestOsRegistryService : public KeyedService {
 public:
  class Registration {
   public:
    Registration(const std::string app_id, const base::Value pref);
    Registration(Registration&& registration) = default;
    Registration& operator=(Registration&& registration) = default;

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;

    ~Registration();

    std::string app_id() const { return app_id_; }
    std::string DesktopFileId() const;
    VmType VmType() const;
    std::string VmName() const;
    std::string ContainerName() const;

    std::string Name() const;
    std::string Exec() const;
    std::string ExecutableFileName() const;
    std::set<std::string> Extensions() const;
    std::set<std::string> MimeTypes() const;
    std::set<std::string> Keywords() const;
    bool NoDisplay() const;
    bool Terminal() const;

    std::string PackageId() const;

    base::Time InstallTime() const;
    base::Time LastLaunchTime() const;

    // Whether this app should scale up when displayed.
    bool IsScaled() const;
    bool CanUninstall() const;

    guest_os::GuestId ToGuestId() const;

    std::string StartupWmClass() const;
    bool StartupNotify() const;

   private:
    std::string GetString(std::string_view key) const;
    bool GetBool(std::string_view key) const;
    base::Time GetTime(std::string_view key) const;
    std::string GetLocalizedString(std::string_view key) const;
    std::set<std::string> GetLocalizedList(std::string_view key) const;

    std::string app_id_;
    base::Value pref_;
  };

  class Observer {
   public:
    // Called at the end of UpdateApplicationList() with lists of app_ids for
    // apps which have been updated, removed, and inserted. Not called when the
    // last_launch_time field is updated.
    virtual void OnRegistryUpdated(
        guest_os::GuestOsRegistryService* registry_service,
        VmType vm_type,
        const std::vector<std::string>& updated_apps,
        const std::vector<std::string>& removed_apps,
        const std::vector<std::string>& inserted_apps) {}

    // Called at the end of AppLaunched().
    virtual void OnAppLastLaunchTimeUpdated(
        VmType vm_type,
        const std::string& app_id,
        const base::Time& last_launch_time) {}

   protected:
    virtual ~Observer() = default;
  };

  explicit GuestOsRegistryService(Profile* profile);

  GuestOsRegistryService(const GuestOsRegistryService&) = delete;
  GuestOsRegistryService& operator=(const GuestOsRegistryService&) = delete;

  ~GuestOsRegistryService() override;

  base::WeakPtr<GuestOsRegistryService> GetWeakPtr();

  // Return all installed apps. This always includes the Terminal app.
  std::map<std::string, GuestOsRegistryService::Registration>
  GetAllRegisteredApps() const;

  // Return all installed apps where the VM is enabled.
  std::map<std::string, GuestOsRegistryService::Registration> GetEnabledApps()
      const;

  // Return all installed apps for a given vm.
  // If |vm_type == TERMINA_VM| then this includes the Terminal app.
  std::map<std::string, GuestOsRegistryService::Registration> GetRegisteredApps(
      VmType vm_type) const;

  // Return null if `app_id` is not found in the registry.
  std::optional<GuestOsRegistryService::Registration> GetRegistration(
      const std::string& app_id) const;

  // Return the preferred handler for the given URL, if any.
  std::optional<GuestOsUrlHandler> GetHandler(const GURL& url) const;

  // Register a non-app handler of URLs.
  // Handlers registered here take priority over apps (since they come from
  // the OS, rather than VMs), and are not persisted to prefs.
  // `canHandleCallback` should return true when passed a URL that should be
  // handled by `handler`.
  void RegisterTransientUrlHandler(GuestOsUrlHandler handler,
                                   CanHandleUrlCallback canHandleCallback);

  // Constructs path to app icon for specific scale factor.
  base::FilePath GetIconPath(const std::string& app_id,
                             ui::ResourceScaleFactor scale_factor) const;

  // Attempts to load icon in the following order:
  // 1/ Loads from resource if |icon_key->resource_id| is valid (non-zero).
  // 2/ Looks up file cache.
  // 3/ Fetches from VM.
  // 4/ Uses |fallback_icon_resource_id| if it is valid (non-zero).
  // 5/ Returns empty.
  void LoadIcon(const std::string& app_id,
                const apps::IconKey& icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                int fallback_icon_resource_id,
                apps::LoadIconCallback callback);

  void LoadIconFromVM(const std::string& app_id,
                      apps::IconType icon_type,
                      int32_t size_hint_in_dip,
                      ui::ResourceScaleFactor scale_factor,
                      apps::IconEffects icon_effects,
                      int fallback_icon_resource_id,
                      apps::LoadIconCallback callback);

  void OnLoadIconFromVM(const std::string& app_id,
                        apps::IconType icon_type,
                        int32_t size_hint_in_dip,
                        apps::IconEffects icon_effects,
                        int fallback_icon_resource_id,
                        apps::LoadIconCallback callback,
                        std::string compressed_icon_data);

  // Fetches icons from container.
  void RequestIcon(const std::string& app_id,
                   ui::ResourceScaleFactor scale_factor,
                   IconContentCallback callback);

  // Remove all apps from the named VM and container. If |container_name| is an
  // empty string, this function removes all apps associated with the VM,
  // regardless of container. Used in the uninstall process.
  void ClearApplicationList(VmType vm_type,
                            const std::string& vm_name,
                            const std::string& container_name);

  // Remove all apps from the named container. Used when deleting a container
  // without deleting the whole VM.
  void ClearApplicationListForContainer(VmType vm_type,
                                        const std::string& vm_name,
                                        const std::string& container_name);

  // The existing list of apps is replaced by |application_list|.
  void UpdateApplicationList(const vm_tools::apps::ApplicationList& app_list);

  // Inform the registry that the badge color for `container_id` has changed. In
  // practice, this sends an update notification for all apps associated with
  // this container, which will prompt the icons to be regenerated.
  void ContainerBadgeColorChanged(const guest_os::GuestId& container_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notify the registry to update the last_launched field.
  void AppLaunched(const std::string& app_id);

  // Serializes the current time and stores it in |dictionary|.
  void SetCurrentTime(base::Value::Dict& dictionary, const char* key) const;

  // Set the display scaled setting of the |app_id| to |scaled|.
  void SetAppScaled(const std::string& app_id, bool scaled);

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }

  // Apply a coloured badge to the app icon if Crostini multi-container
  // feature is enabled.
  void ApplyContainerBadge(const std::optional<std::string>& app_id,
                           gfx::ImageSkia* image_skia);

  // Returns the AppId that will be used to refer to the given GuestOs
  // application.
  static std::string GenerateAppId(const std::string& desktop_file_id,
                                   const std::string& vm_name,
                                   const std::string& container_name);

 private:
  // Construct path to app local data.
  base::FilePath GetAppPath(const std::string& app_id) const;
  // Called to request an icon from the container.
  void RequestContainerAppIcon(const std::string& app_id,
                               ui::ResourceScaleFactor scale_factor);
  // Callback for when we request an icon from the container.
  void OnContainerAppIcon(const std::string& app_id,
                          ui::ResourceScaleFactor scale_factor,
                          bool success,
                          const std::vector<crostini::Icon>& icons);
  // Removes all the icons installed for an application.
  void RemoveAppData(const std::string& app_id);

  // Apply container-specific badging to `icon_out`. This is used by
  // ApplyContainerBadge.
  void ApplyContainerBadgeForImageSkiaIcon(SkColor badge_color,
                                           gfx::ImageSkia* icon_out);

  // Apply container-specific badging to `icon` before running the callback.
  // This is run after the generic icon loading code.
  void ApplyContainerBadgeWithCallback(SkColor badge_color,
                                       apps::LoadIconCallback callback,
                                       apps::IconValuePtr icon);

  // Call the callbacks |active_icon_requests_| for |app_id|.
  void InvokeActiveIconCallbacks(std::string app_id,
                                 ui::ResourceScaleFactor scale_factor,
                                 std::string icon_content);

  // If a valid .svg file is found at |svg_path|, transcode it to png and save
  // it to |png_path| and invoke |callback|, otherwise invoke |fallback|.
  void TranscodeIconFromSvg(
      base::FilePath svg_path,
      base::FilePath png_path,
      apps::IconType icon_type,
      int32_t size_hint_in_dip,
      apps::IconEffects icon_effects,
      base::OnceCallback<void(apps::LoadIconCallback)> fallback,
      apps::LoadIconCallback callback);

  // Callback for when a saved container icon is svg and was transcoded.
  void OnSvgIconTranscoded(std::string app_id,
                           ui::ResourceScaleFactor scale_factor,
                           std::string svg_icon_content,
                           std::string png_icon_content);

  // Owned by the Profile.
  const raw_ptr<Profile, DanglingUntriaged> profile_;
  const raw_ptr<PrefService, DanglingUntriaged> prefs_;

  // Keeps root folder where Crostini app icons for different scale factors are
  // stored.
  base::FilePath base_icon_path_;

  base::ObserverList<Observer>::Unchecked observers_;

  raw_ptr<const base::Clock> clock_;

  std::vector<std::pair<GuestOsUrlHandler, CanHandleUrlCallback>> url_handlers_;

  // Keeps record for icon request to avoid duplication. Each app may contain
  // several requests for different scale factors. Scale factor is defined by
  // specific bit position. The |active_icon_requests_| holds icon request that
  // are in flight. |retry_icon_requests| holds failed requests which we
  // should attempt again when we get an app list refresh from the container
  // which means there's a good chance the container is online and the request
  // will then succeed.
  std::map<std::pair<std::string, ui::ResourceScaleFactor>,
           std::vector<IconContentCallback>>
      active_icon_requests_;
  std::map<std::string, uint32_t> retry_icon_requests_;

  std::unique_ptr<apps::SvgIconTranscoder> svg_icon_transcoder_;
  base::WeakPtrFactory<GuestOsRegistryService> weak_ptr_factory_{this};
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_REGISTRY_SERVICE_H_
