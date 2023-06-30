// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/version.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/model/sync_change.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

namespace syncer {
class SyncData;
}

namespace sync_pb {
class AppSpecifics;
class ExtensionSpecifics;
}

namespace extensions {

class Extension;

// A class that encapsulates the synced properties of an App or Extension.
// Corresponds to an ExtensionSpecifics or an AppSpecifics proto (note that an
// AppSpecifics itself includes an ExtensionSpecifics).
class ExtensionSyncData {
 public:
  struct LinkedAppIconInfo {
    LinkedAppIconInfo();
    ~LinkedAppIconInfo();

    GURL url;
    int size;
  };

  // Extension constructor.
  ExtensionSyncData(const Extension& extension,
                    bool enabled,
                    int disable_reasons,
                    bool incognito_enabled,
                    bool remote_install,
                    const GURL& update_url);
  // App constructor.
  ExtensionSyncData(const Extension& extension,
                    bool enabled,
                    int disable_reasons,
                    bool incognito_enabled,
                    bool remote_install,
                    const GURL& update_url,
                    const syncer::StringOrdinal& app_launch_ordinal,
                    const syncer::StringOrdinal& page_ordinal,
                    extensions::LaunchType launch_type);
  ExtensionSyncData(const ExtensionSyncData& other);
  ~ExtensionSyncData();

  // For constructing an ExtensionSyncData from received sync data.
  // May return null if the sync data was invalid.
  static std::unique_ptr<ExtensionSyncData> CreateFromSyncData(
      const syncer::SyncData& sync_data);
  static std::unique_ptr<ExtensionSyncData> CreateFromSyncChange(
      const syncer::SyncChange& sync_change);

  // Retrieve sync data from this class.
  syncer::SyncData GetSyncData() const;
  syncer::SyncChange GetSyncChange(
      syncer::SyncChange::SyncChangeType change_type) const;

  bool is_app() const { return is_app_; }

  const std::string& id() const { return id_; }

  // Version-independent properties (i.e., used even when the version of the
  // currently-installed extension doesn't match |version|).
  bool uninstalled() const { return uninstalled_; }
  bool enabled() const { return enabled_; }
  void set_enabled(bool enabled) { enabled_ = enabled; }
  bool supports_disable_reasons() const { return supports_disable_reasons_; }
  int disable_reasons() const { return disable_reasons_; }
  bool incognito_enabled() const { return incognito_enabled_; }
  bool remote_install() const { return remote_install_; }

  // Version-dependent properties (i.e., should be used only when the
  // version of the currently-installed extension matches |version|).
  const base::Version& version() const { return version_; }
  void set_version(const base::Version& version) { version_ = version; }
  const GURL& update_url() const { return update_url_; }
  // Used only for debugging.
  const std::string& name() const { return name_; }

  // Everything below is App-specific - only set for Apps, not Extensions.

  // These ordinals aren't necessarily valid. Some applications don't have
  // valid ordinals because they don't appear on the new tab page.
  const syncer::StringOrdinal& app_launch_ordinal() const {
    return app_launch_ordinal_;
  }
  const syncer::StringOrdinal& page_ordinal() const { return page_ordinal_; }
  extensions::LaunchType launch_type() const { return launch_type_; }
  const std::vector<LinkedAppIconInfo>& linked_icons() const {
    return linked_icons_;
  }

  bool is_deprecated_bookmark_app() const {
    return is_deprecated_bookmark_app_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionSyncDataTest,
                           ExtensionSyncDataForExtension);

  ExtensionSyncData();

  // Populate this class from sync inputs. Return true if the input was valid.
  bool PopulateFromSyncData(const syncer::SyncData& sync_data);
  bool PopulateFromExtensionSpecifics(
      const sync_pb::ExtensionSpecifics& specifics);
  bool PopulateFromAppSpecifics(const sync_pb::AppSpecifics& specifics);

  // Convert an ExtensionSyncData back out to a sync ExtensionSpecifics.
  void ToExtensionSpecifics(sync_pb::ExtensionSpecifics* specifics) const;

  // Convert an ExtensionSyncData back out to a sync AppSpecifics.
  void ToAppSpecifics(sync_pb::AppSpecifics* specifics) const;

  bool is_app_;

  std::string id_;
  bool uninstalled_;
  bool enabled_;
  // |supports_disable_reasons_| is true if the optional |disable_reasons_| was
  // set to some value in the extension_specifics.proto. If not,
  // |disable_reasons_| is given a default value and |supports_disable_reasons_|
  // is false.
  bool supports_disable_reasons_;
  int disable_reasons_;
  bool incognito_enabled_;
  bool remote_install_;
  base::Version version_;
  GURL update_url_;
  std::string name_;

  // App-specific fields.
  syncer::StringOrdinal app_launch_ordinal_;
  syncer::StringOrdinal page_ordinal_;
  extensions::LaunchType launch_type_;
  std::vector<LinkedAppIconInfo> linked_icons_;
  bool is_deprecated_bookmark_app_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
