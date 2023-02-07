// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry registers pictures directories and media devices as
// File API filesystems and keeps track of the path to filesystem ID mappings.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/storage_monitor/removable_storage_observer.h"

class BrowserContextKeyedServiceShutdownNotifierFactory;
class ExtensionGalleriesHost;
class GalleryWatchManager;
class MediaFileSystemContext;
class MediaGalleriesPreferences;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

// Contains information about a particular filesystem being provided to a
// client, including metadata like the name and ID, and API handles like the
// fsid (filesystem ID) used to hook up the API objects.
struct MediaFileSystemInfo {
  MediaFileSystemInfo(const std::u16string& fs_name,
                      const base::FilePath& fs_path,
                      const std::string& filesystem_id,
                      MediaGalleryPrefId pref_id,
                      const std::string& transient_device_id,
                      bool removable,
                      bool media_device);
  MediaFileSystemInfo();
  MediaFileSystemInfo(const MediaFileSystemInfo& other);
  ~MediaFileSystemInfo();

  std::u16string name;
  base::FilePath path;
  std::string fsid;
  MediaGalleryPrefId pref_id;
  std::string transient_device_id;
  bool removable;
  bool media_device;
};

typedef base::OnceCallback<void(const std::vector<MediaFileSystemInfo>&)>
    MediaFileSystemsCallback;

// Tracks usage of filesystems by extensions.
// This object lives on the UI thread.
class MediaFileSystemRegistry
    : public storage_monitor::RemovableStorageObserver,
      public MediaGalleriesPreferences::GalleryChangeObserver {
 public:
  MediaFileSystemRegistry();

  MediaFileSystemRegistry(const MediaFileSystemRegistry&) = delete;
  MediaFileSystemRegistry& operator=(const MediaFileSystemRegistry&) = delete;

  ~MediaFileSystemRegistry() override;

  // Passes to |callback| the list of media filesystem IDs and paths for a
  // given WebContents.
  void GetMediaFileSystemsForExtension(content::WebContents* contents,
                                       const extensions::Extension* extension,
                                       MediaFileSystemsCallback callback);

  // Attempt to register the file system for |pref_id|. If |extension| does not
  // have permission to |pref_id|, sends |callback| FILE_ERROR_NOT_FOUND.
  void RegisterMediaFileSystemForExtension(
      content::WebContents* contents,
      const extensions::Extension* extension,
      MediaGalleryPrefId pref_id,
      base::OnceCallback<void(base::File::Error result)> callback);

  // Returns the media galleries preferences for the specified |profile|.
  // Caller is responsible for ensuring that the preferences are initialized
  // before use.
  MediaGalleriesPreferences* GetPreferences(Profile* profile);

  GalleryWatchManager* gallery_watch_manager();

  // RemovableStorageObserver implementation.
  void OnRemovableStorageDetached(
      const storage_monitor::StorageInfo& info) override;

  static BrowserContextKeyedServiceShutdownNotifierFactory*
  GetFactoryInstance();

 private:
  class MediaFileSystemContextImpl;

  friend class MediaFileSystemContextImpl;
  friend class MediaFileSystemRegistryTest;
  friend class TestMediaFileSystemContext;

  // Map an extension to the ExtensionGalleriesHost.
  typedef std::map<std::string /*extension_id*/,
                   std::unique_ptr<ExtensionGalleriesHost>>
      ExtensionHostMap;
  // Map a profile and extension to the ExtensionGalleriesHost.
  typedef std::map<Profile*, ExtensionHostMap> ExtensionGalleriesHostMap;
  // Map a profile to a shutdown notification subscription.
  typedef std::map<Profile*, base::CallbackListSubscription>
      ProfileSubscriptionMap;

  void OnPermissionRemoved(MediaGalleriesPreferences* pref,
                           const std::string& extension_id,
                           MediaGalleryPrefId pref_id) override;
  void OnGalleryRemoved(MediaGalleriesPreferences* pref,
                        MediaGalleryPrefId pref_id) override;

  // Look up or create the extension gallery host.
  ExtensionGalleriesHost* GetExtensionGalleryHost(
      Profile* profile,
      MediaGalleriesPreferences* preferences,
      const std::string& extension_id);

  void OnExtensionGalleriesHostEmpty(Profile* profile,
                                     const std::string& extension_id);

  void OnProfileShutdown(Profile* profile);

  // This map owns all the ExtensionGalleriesHost objects created.
  ExtensionGalleriesHostMap extension_hosts_map_;

  // The above map uses raw Profile pointers as keys. This map removes those
  // entries when the Profile is destroyed.
  ProfileSubscriptionMap profile_subscription_map_;

  std::unique_ptr<MediaFileSystemContext> file_system_context_;

  std::unique_ptr<GalleryWatchManager> gallery_watch_manager_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_REGISTRY_H_
