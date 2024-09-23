// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_file_system_registry.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/gallery_watch_manager.h"
#include "chrome/browser/media_galleries/media_file_system_context.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#endif

using content::BrowserThread;
using content::NavigationController;
using content::RenderProcessHost;
using content::WebContents;
using storage::ExternalMountPoints;
using storage_monitor::MediaStorageUtil;
using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;

namespace {

class MediaFileSystemRegistryShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static MediaFileSystemRegistryShutdownNotifierFactory* GetInstance() {
    return base::Singleton<
        MediaFileSystemRegistryShutdownNotifierFactory>::get();
  }

  MediaFileSystemRegistryShutdownNotifierFactory(
      const MediaFileSystemRegistryShutdownNotifierFactory&) = delete;
  MediaFileSystemRegistryShutdownNotifierFactory& operator=(
      const MediaFileSystemRegistryShutdownNotifierFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      MediaFileSystemRegistryShutdownNotifierFactory>;

  MediaFileSystemRegistryShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "MediaFileSystemRegistry") {
    DependsOn(MediaGalleriesPreferencesFactory::GetInstance());
  }
  ~MediaFileSystemRegistryShutdownNotifierFactory() override {}
};

struct InvalidatedGalleriesInfo {
  std::set<raw_ptr<ExtensionGalleriesHost, SetExperimental>> extension_hosts;
  std::set<MediaGalleryPrefId> pref_ids;
};

// Just a convenience class to help scope ExtensionGalleriesHost to the lifetime
// of a content::Page.
class PageAlivenessReference
    : public content::PageUserData<PageAlivenessReference> {
 public:
  // `page_dead_callback` is called when the page is destroyed.
  PageAlivenessReference(content::Page& page,
                         base::OnceClosure page_dead_callback)
      : content::PageUserData<PageAlivenessReference>(page),
        page_dead_callback_(std::move(page_dead_callback)) {}
  ~PageAlivenessReference() override { std::move(page_dead_callback_).Run(); }

 private:
  friend PageUserData;
  PAGE_USER_DATA_KEY_DECL();

  // Called once the page is dead.
  base::OnceClosure page_dead_callback_;
};

PAGE_USER_DATA_KEY_IMPL(PageAlivenessReference);

}  // namespace

MediaFileSystemInfo::MediaFileSystemInfo(const std::u16string& fs_name,
                                         const base::FilePath& fs_path,
                                         const std::string& filesystem_id,
                                         MediaGalleryPrefId pref_id,
                                         const std::string& transient_device_id,
                                         bool removable,
                                         bool media_device)
    : name(fs_name),
      path(fs_path),
      fsid(filesystem_id),
      pref_id(pref_id),
      transient_device_id(transient_device_id),
      removable(removable),
      media_device(media_device) {}

MediaFileSystemInfo::MediaFileSystemInfo() {}
MediaFileSystemInfo::MediaFileSystemInfo(const MediaFileSystemInfo& other) =
    default;
MediaFileSystemInfo::~MediaFileSystemInfo() {}

// The main owner of this class is
// |MediaFileSystemRegistry::extension_hosts_map_|, but a callback may
// temporarily hold a reference.
class ExtensionGalleriesHost {
 public:
  // |no_references_callback| is called when the last WebContents reference
  // goes away. WebContents references are added through
  // ReferenceFromWebContents().
  ExtensionGalleriesHost(MediaFileSystemContext* file_system_context,
                         const base::FilePath& profile_path,
                         const std::string& extension_id,
                         base::OnceClosure no_references_callback)
      : file_system_context_(file_system_context),
        profile_path_(profile_path),
        extension_id_(extension_id),
        no_references_callback_(std::move(no_references_callback)) {}

  ~ExtensionGalleriesHost() {
    for (auto& it : pref_id_map_) {
      file_system_context_->RevokeFileSystem(it.second.fsid);
    }
  }

  ExtensionGalleriesHost(const ExtensionGalleriesHost&) = delete;
  ExtensionGalleriesHost& operator=(const ExtensionGalleriesHost&) = delete;

  // For each gallery in the list of permitted |galleries|, checks if the
  // device is attached and if so looks up or creates a file system name and
  // passes the information needed for the renderer to create those file
  // system objects to the |callback|.
  void GetMediaFileSystems(const MediaGalleryPrefIdSet& galleries,
                           const MediaGalleriesPrefInfoMap& galleries_info,
                           MediaFileSystemsCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Extract all the device ids so we can make sure they are attached.
    MediaStorageUtil::DeviceIdSet* device_ids =
        new MediaStorageUtil::DeviceIdSet;
    for (auto id = galleries.begin(); id != galleries.end(); ++id) {
      device_ids->insert(galleries_info.find(*id)->second.device_id);
    }
    MediaStorageUtil::FilterAttachedDevices(
        device_ids,
        base::BindOnce(
            &ExtensionGalleriesHost::GetMediaFileSystemsForAttachedDevices,
            weak_factory_.GetWeakPtr(), base::Owned(device_ids), galleries,
            galleries_info, std::move(callback)));
  }

  // Checks if |gallery| is attached and if so, registers the file system and
  // then calls |callback| with the result.
  void RegisterMediaFileSystem(
      const MediaGalleryPrefInfo& gallery,
      base::OnceCallback<void(base::File::Error result)> callback) {
    // Extract all the device ids so we can make sure they are attached.
    MediaStorageUtil::DeviceIdSet* device_ids =
        new MediaStorageUtil::DeviceIdSet;
    device_ids->insert(gallery.device_id);
    MediaStorageUtil::FilterAttachedDevices(
        device_ids,
        base::BindOnce(&ExtensionGalleriesHost::RegisterAttachedMediaFileSystem,
                       weak_factory_.GetWeakPtr(), base::Owned(device_ids),
                       gallery, std::move(callback)));
  }

  // Revoke the file system for |id| if this extension has created one for |id|.
  void RevokeGalleryByPrefId(MediaGalleryPrefId id) {
    auto gallery = pref_id_map_.find(id);
    if (gallery == pref_id_map_.end())
      return;

    file_system_context_->RevokeFileSystem(gallery->second.fsid);
    pref_id_map_.erase(gallery);

    if (pref_id_map_.empty()) {
      CleanUp();
    }
  }

  // Indicate that the passed |contents| will reference the file system ids
  // created
  // by this class.
  void ReferenceFromWebContents(content::WebContents* web_contents) {
    referring_page_count_++;
    // The count is decremented when the below Page is destroyed.
    PageAlivenessReference::CreateForPage(
        web_contents->GetPrimaryPage(),
        base::BindOnce(&ExtensionGalleriesHost::DecrementReferringPageCount,
                       weak_factory_.GetWeakPtr()));
  }

  void DecrementReferringPageCount() {
    referring_page_count_--;
    if (referring_page_count_ == 0) {
      CleanUp();
    }
  }

 private:
  typedef std::map<MediaGalleryPrefId, MediaFileSystemInfo> PrefIdFsInfoMap;

  void GetMediaFileSystemsForAttachedDevices(
      const MediaStorageUtil::DeviceIdSet* attached_devices,
      const MediaGalleryPrefIdSet& galleries,
      const MediaGalleriesPrefInfoMap& galleries_info,
      MediaFileSystemsCallback callback) {
    std::vector<MediaFileSystemInfo> result;

    for (auto pref_id_it = galleries.begin(); pref_id_it != galleries.end();
         ++pref_id_it) {
      const MediaGalleryPrefId& pref_id = *pref_id_it;
      const MediaGalleryPrefInfo& gallery_info =
          galleries_info.find(pref_id)->second;
      const std::string& device_id = gallery_info.device_id;
      if (!base::Contains(*attached_devices, device_id))
        continue;

      PrefIdFsInfoMap::const_iterator existing_info =
          pref_id_map_.find(pref_id);
      if (existing_info != pref_id_map_.end()) {
        result.push_back(existing_info->second);
        continue;
      }

      base::FilePath path = gallery_info.AbsolutePath();
      if (!MediaStorageUtil::CanCreateFileSystem(device_id, path))
        continue;

      std::string fs_name = MediaFileSystemBackend::ConstructMountName(
          profile_path_, extension_id_, pref_id);
      if (!file_system_context_->RegisterFileSystem(device_id, fs_name, path))
        continue;

      MediaFileSystemInfo new_entry(
          gallery_info.GetGalleryDisplayName(),
          file_system_context_->GetRegisteredPath(fs_name),
          fs_name,
          pref_id,
          GetTransientIdForRemovableDeviceId(device_id),
          StorageInfo::IsRemovableDevice(device_id),
          StorageInfo::IsMediaDevice(device_id));
      result.push_back(new_entry);
      pref_id_map_[pref_id] = new_entry;
    }

    if (result.empty()) {
      CleanUp();
    }

    DCHECK_EQ(pref_id_map_.size(), result.size());
    std::move(callback).Run(result);
  }

  void RegisterAttachedMediaFileSystem(
      const MediaStorageUtil::DeviceIdSet* attached_device,
      const MediaGalleryPrefInfo& gallery,
      base::OnceCallback<void(base::File::Error result)> callback) {
    base::File::Error result = base::File::FILE_ERROR_NOT_FOUND;

    if (!attached_device->empty()) {
      std::string fs_name = MediaFileSystemBackend::ConstructMountName(
          profile_path_, extension_id_, gallery.pref_id);
      base::FilePath path = gallery.AbsolutePath();
      const std::string& device_id = gallery.device_id;

      if (base::Contains(pref_id_map_, gallery.pref_id)) {
        result = base::File::FILE_OK;
      } else if (MediaStorageUtil::CanCreateFileSystem(device_id, path) &&
                 file_system_context_->RegisterFileSystem(device_id, fs_name,
                                                          path)) {
        result = base::File::FILE_OK;
        pref_id_map_[gallery.pref_id] = MediaFileSystemInfo(
            gallery.GetGalleryDisplayName(),
            file_system_context_->GetRegisteredPath(fs_name),
            fs_name,
            gallery.pref_id,
            GetTransientIdForRemovableDeviceId(device_id),
            StorageInfo::IsRemovableDevice(device_id),
            StorageInfo::IsMediaDevice(device_id));
      }
    }

    if (pref_id_map_.empty()) {
      CleanUp();
    }
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

  std::string GetTransientIdForRemovableDeviceId(const std::string& device_id) {
    if (!StorageInfo::IsRemovableDevice(device_id))
      return std::string();

    return StorageMonitor::GetInstance()->GetTransientIdForDeviceId(device_id);
  }

  void CleanUp() {
    // This causes the owner of this class to destroy this object. The revoking
    // of our filesystems occurs in the destructor. It's written that way
    // because sometimes the Profile Shutdown can ALSO cause object destruction.
    std::move(no_references_callback_).Run();
  }

  // MediaFileSystemRegistry owns |this| and |file_system_context_|, so it's
  // safe to store a raw pointer.
  raw_ptr<MediaFileSystemContext> file_system_context_;

  // Path for the active profile.
  const base::FilePath profile_path_;

  // Id of the extension this host belongs to.
  const extensions::ExtensionId extension_id_;

  // A callback to call when the last WebContents reference goes away.
  base::OnceClosure no_references_callback_;

  // A map from the gallery preferences id to the file system information.
  PrefIdFsInfoMap pref_id_map_;

  // The number of living content::Page instances that are interested in this
  // combination of Profile and `extension_id`.
  size_t referring_page_count_ = 0U;

  // Don't let callbacks extend our lifetime, as that makes things complicated.
  base::WeakPtrFactory<ExtensionGalleriesHost> weak_factory_{this};
};

/******************
 * Public methods
 ******************/

void MediaFileSystemRegistry::GetMediaFileSystemsForExtension(
    content::WebContents* contents,
    const extensions::Extension* extension,
    MediaFileSystemsCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  MediaGalleriesPreferences* preferences = GetPreferences(profile);
  MediaGalleryPrefIdSet galleries =
      preferences->GalleriesForExtension(*extension);

  if (galleries.empty()) {
    std::move(callback).Run(std::vector<MediaFileSystemInfo>());
    return;
  }

  ExtensionGalleriesHost* extension_host =
      GetExtensionGalleryHost(profile, preferences, extension->id());

  // This must come before the GetMediaFileSystems call to make sure the
  // contents of the context is referenced before the filesystems are retrieved.
  extension_host->ReferenceFromWebContents(contents);

  extension_host->GetMediaFileSystems(galleries, preferences->known_galleries(),
                                      std::move(callback));
}

void MediaFileSystemRegistry::RegisterMediaFileSystemForExtension(
    content::WebContents* contents,
    const extensions::Extension* extension,
    MediaGalleryPrefId pref_id,
    base::OnceCallback<void(base::File::Error result)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(kInvalidMediaGalleryPrefId, pref_id);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  MediaGalleriesPreferences* preferences = GetPreferences(profile);
  auto gallery = preferences->known_galleries().find(pref_id);
  MediaGalleryPrefIdSet permitted_galleries =
      preferences->GalleriesForExtension(*extension);

  if (gallery == preferences->known_galleries().end() ||
      !base::Contains(permitted_galleries, pref_id)) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::File::FILE_ERROR_NOT_FOUND));
    return;
  }

  ExtensionGalleriesHost* extension_host =
      GetExtensionGalleryHost(profile, preferences, extension->id());

  // This must come before the GetMediaFileSystems call to make sure the
  // contents of the context is referenced before the filesystems are retrieved.
  extension_host->ReferenceFromWebContents(contents);

  extension_host->RegisterMediaFileSystem(gallery->second, std::move(callback));
}

MediaGalleriesPreferences* MediaFileSystemRegistry::GetPreferences(
    Profile* profile) {
  // Create an empty ExtensionHostMap for this profile on first initialization.
  if (!base::Contains(extension_hosts_map_, profile)) {
    extension_hosts_map_[profile] = ExtensionHostMap();
    DCHECK(!base::Contains(profile_subscription_map_, profile));
    profile_subscription_map_[profile] =
        MediaFileSystemRegistryShutdownNotifierFactory::GetInstance()
            ->Get(profile)
            ->Subscribe(
                base::BindRepeating(&MediaFileSystemRegistry::OnProfileShutdown,
                                    base::Unretained(this), profile));
  }

  return MediaGalleriesPreferencesFactory::GetForProfile(profile);
}

GalleryWatchManager* MediaFileSystemRegistry::gallery_watch_manager() {
  if (!gallery_watch_manager_)
    gallery_watch_manager_ = std::make_unique<GalleryWatchManager>();
  return gallery_watch_manager_.get();
}

void MediaFileSystemRegistry::OnRemovableStorageDetached(
    const StorageInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Since revoking a gallery in the ExtensionGalleriesHost may cause it
  // to be removed from the map and therefore invalidate any iterator pointing
  // to it, this code first copies all the invalid gallery ids and the
  // extension hosts in which they may appear (per profile) and revoked it in
  // a second step.
  std::vector<InvalidatedGalleriesInfo> invalid_galleries_info;

  for (auto profile_it = extension_hosts_map_.begin();
       profile_it != extension_hosts_map_.end(); ++profile_it) {
    MediaGalleriesPreferences* preferences = GetPreferences(profile_it->first);
    // If |preferences| is not yet initialized, it won't contain any galleries.
    if (!preferences->IsInitialized())
      continue;

    InvalidatedGalleriesInfo invalid_galleries_in_profile;
    invalid_galleries_in_profile.pref_ids =
        preferences->LookUpGalleriesByDeviceId(info.device_id());

    for (ExtensionHostMap::const_iterator extension_host_it =
             profile_it->second.begin();
         extension_host_it != profile_it->second.end();
         ++extension_host_it) {
      invalid_galleries_in_profile.extension_hosts.insert(
          extension_host_it->second.get());
    }

    invalid_galleries_info.push_back(invalid_galleries_in_profile);
  }

  for (size_t i = 0; i < invalid_galleries_info.size(); i++) {
    for (auto extension_host_it =
             invalid_galleries_info[i].extension_hosts.begin();
         extension_host_it != invalid_galleries_info[i].extension_hosts.end();
         ++extension_host_it) {
      for (auto pref_id_it = invalid_galleries_info[i].pref_ids.begin();
           pref_id_it != invalid_galleries_info[i].pref_ids.end();
           ++pref_id_it) {
        (*extension_host_it)->RevokeGalleryByPrefId(*pref_id_it);
      }
    }
  }
}

/******************
 * Private methods
 ******************/

class MediaFileSystemRegistry::MediaFileSystemContextImpl
    : public MediaFileSystemContext {
 public:
  MediaFileSystemContextImpl() {}

  MediaFileSystemContextImpl(const MediaFileSystemContextImpl&) = delete;
  MediaFileSystemContextImpl& operator=(const MediaFileSystemContextImpl&) =
      delete;

  ~MediaFileSystemContextImpl() override {}

  bool RegisterFileSystem(const std::string& device_id,
                          const std::string& fs_name,
                          const base::FilePath& path) override {
    if (StorageInfo::IsMassStorageDevice(device_id))
      return RegisterFileSystemForMassStorage(device_id, fs_name, path);
    return RegisterFileSystemForMTPDevice(device_id, fs_name, path);
  }

  void RevokeFileSystem(const std::string& fs_name) override {
    ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(fs_name);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MTPDeviceMapService::RevokeMTPFileSystem,
                       base::Unretained(MTPDeviceMapService::GetInstance()),
                       fs_name));
#endif
  }

  base::FilePath GetRegisteredPath(const std::string& fs_name) const override {
    base::FilePath result;
    if (!ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(fs_name,
                                                                     &result)) {
      return base::FilePath();
    }
    return result;
  }

 private:
  // Registers and returns the file system id for the mass storage device
  // specified by |device_id| and |path|.
  bool RegisterFileSystemForMassStorage(const std::string& device_id,
                                        const std::string& fs_name,
                                        const base::FilePath& path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(StorageInfo::IsMassStorageDevice(device_id));

    // Sanity checks for |path|.
    CHECK(path.IsAbsolute());
    CHECK(!path.ReferencesParent());

    return ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        fs_name, storage::kFileSystemTypeLocalMedia,
        storage::FileSystemMountOption(), path);
  }

  bool RegisterFileSystemForMTPDevice(const std::string& device_id,
                                      const std::string fs_name,
                                      const base::FilePath& path) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!StorageInfo::IsMassStorageDevice(device_id));

    // Sanity checks for |path|.
    CHECK(MediaStorageUtil::CanCreateFileSystem(device_id, path));
    bool result = ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        fs_name,
        storage::kFileSystemTypeDeviceMedia,
        storage::FileSystemMountOption(),
        path);
    CHECK(result);
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MTPDeviceMapService::RegisterMTPFileSystem,
                       base::Unretained(MTPDeviceMapService::GetInstance()),
                       path.value(), fs_name, true /* read only */));
    return result;
#else
    NOTREACHED_IN_MIGRATION();
    return false;
#endif
  }
};

// Constructor in 'private' section because depends on private class definition.
MediaFileSystemRegistry::MediaFileSystemRegistry()
    : file_system_context_(new MediaFileSystemContextImpl) {
  StorageMonitor::GetInstance()->AddObserver(this);
}

MediaFileSystemRegistry::~MediaFileSystemRegistry() {
  DCHECK(StorageMonitor::GetInstance());
  StorageMonitor::GetInstance()->RemoveObserver(this);
}

void MediaFileSystemRegistry::OnPermissionRemoved(
    MediaGalleriesPreferences* prefs,
    const std::string& extension_id,
    MediaGalleryPrefId pref_id) {
  Profile* profile = prefs->profile();
  ExtensionGalleriesHostMap::const_iterator host_map_it =
      extension_hosts_map_.find(profile);
  CHECK(host_map_it != extension_hosts_map_.end(), base::NotFatalUntil::M130);
  const ExtensionHostMap& extension_host_map = host_map_it->second;
  auto gallery_host_it = extension_host_map.find(extension_id);
  if (gallery_host_it == extension_host_map.end())
    return;
  gallery_host_it->second->RevokeGalleryByPrefId(pref_id);
}

void MediaFileSystemRegistry::OnGalleryRemoved(
    MediaGalleriesPreferences* prefs,
    MediaGalleryPrefId pref_id) {
  Profile* profile = prefs->profile();
  // Get the Extensions, MediaGalleriesPreferences and ExtensionHostMap for
  // |profile|.
  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  ExtensionGalleriesHostMap::const_iterator host_map_it =
      extension_hosts_map_.find(profile);
  CHECK(host_map_it != extension_hosts_map_.end(), base::NotFatalUntil::M130);
  const ExtensionHostMap& extension_host_map = host_map_it->second;

  // Go through ExtensionHosts, and remove indicated gallery, if any.
  // RevokeGalleryByPrefId() may end up deleting from |extension_host_map| and
  // even delete |extension_host_map| altogether. So do this in two loops to
  // avoid using an invalidated iterator or deleted map.
  std::vector<const extensions::Extension*> extensions;
  for (auto it = extension_host_map.begin(); it != extension_host_map.end();
       ++it) {
    extensions.push_back(
        extension_registry->enabled_extensions().GetByID(it->first));
  }
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (!base::Contains(extension_hosts_map_, profile))
      break;
    auto gallery_host_it = extension_host_map.find(extensions[i]->id());
    if (gallery_host_it == extension_host_map.end())
      continue;
    gallery_host_it->second->RevokeGalleryByPrefId(pref_id);
  }
}

ExtensionGalleriesHost* MediaFileSystemRegistry::GetExtensionGalleryHost(
    Profile* profile,
    MediaGalleriesPreferences* preferences,
    const std::string& extension_id) {
  auto extension_hosts = extension_hosts_map_.find(profile);
  // GetPreferences(), which had to be called because preferences is an
  // argument, ensures that profile is in the map.
  CHECK(extension_hosts != extension_hosts_map_.end(),
        base::NotFatalUntil::M130);
  if (extension_hosts->second.empty())
    preferences->AddGalleryChangeObserver(this);

  auto it = extension_hosts->second.find(extension_id);
  if (it != extension_hosts->second.end()) {
    return it->second.get();
  }

  auto new_host = std::make_unique<ExtensionGalleriesHost>(
      file_system_context_.get(), profile->GetPath(), extension_id,
      base::BindOnce(&MediaFileSystemRegistry::OnExtensionGalleriesHostEmpty,
                     base::Unretained(this), profile, extension_id));
  // Save a raw pointer to the newly created host so we can return it.
  auto* result = new_host.get();
  extension_hosts->second[extension_id] = std::move(new_host);
  return result;
}

void MediaFileSystemRegistry::OnExtensionGalleriesHostEmpty(
    Profile* profile, const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto extension_hosts = extension_hosts_map_.find(profile);
  CHECK(extension_hosts != extension_hosts_map_.end(),
        base::NotFatalUntil::M130);
  ExtensionHostMap::size_type erase_count =
      extension_hosts->second.erase(extension_id);
  DCHECK_EQ(1U, erase_count);
  if (extension_hosts->second.empty()) {
    // When a profile has no ExtensionGalleriesHosts left, remove the
    // matching gallery-change-watcher since it is no longer needed. Leave the
    // |extension_hosts| entry alone, since it indicates the profile has been
    // previously used.
    MediaGalleriesPreferences* preferences = GetPreferences(profile);
    preferences->RemoveGalleryChangeObserver(this);
  }
}

void MediaFileSystemRegistry::OnProfileShutdown(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto extension_hosts_it = extension_hosts_map_.find(profile);
  CHECK(extension_hosts_it != extension_hosts_map_.end(),
        base::NotFatalUntil::M130);
  extension_hosts_map_.erase(extension_hosts_it);

  auto profile_subscription_it = profile_subscription_map_.find(profile);
  CHECK(profile_subscription_it != profile_subscription_map_.end(),
        base::NotFatalUntil::M130);
  profile_subscription_map_.erase(profile_subscription_it);
}

// static
BrowserContextKeyedServiceShutdownNotifierFactory*
MediaFileSystemRegistry::GetFactoryInstance() {
  return MediaFileSystemRegistryShutdownNotifierFactory::GetInstance();
}
