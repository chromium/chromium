// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_data.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "components/crx_file/id_util.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_url_handlers.h"

using syncer::StringOrdinal;

namespace extensions {

namespace {

std::string GetExtensionSpecificsLogMessage(
    const sync_pb::ExtensionSpecifics& specifics) {
  return base::StringPrintf(
      "id: %s\nversion: %s\nupdate_url: %s\nenabled: %i\ndisable_reasons: %i",
      specifics.id().c_str(),
      specifics.version().c_str(),
      specifics.update_url().c_str(),
      specifics.enabled(),
      specifics.disable_reasons());
}

enum BadSyncDataReason {
  // Invalid extension ID.
  BAD_EXTENSION_ID,

  // Invalid version.
  BAD_VERSION,

  // Invalid update URL.
  BAD_UPDATE_URL,

  // No ExtensionSpecifics in the EntitySpecifics.
  NO_EXTENSION_SPECIFICS,

  // Not used anymore; still here because of UMA.
  DEPRECATED_BAD_DISABLE_REASONS,

  // Must be at the end.
  NUM_BAD_SYNC_DATA_REASONS
};

void RecordBadSyncData(BadSyncDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.BadSyncDataReason", reason,
                            NUM_BAD_SYNC_DATA_REASONS);
}

}  // namespace

ExtensionSyncData::LinkedAppIconInfo::LinkedAppIconInfo() {
}

ExtensionSyncData::LinkedAppIconInfo::~LinkedAppIconInfo() {
}

ExtensionSyncData::ExtensionSyncData()
    : is_app_(false),
      uninstalled_(false),
      enabled_(false),
      supports_disable_reasons_(false),
      disable_reasons_(disable_reason::DISABLE_NONE),
      incognito_enabled_(false),
      remote_install_(false),
      launch_type_(LAUNCH_TYPE_INVALID) {}

ExtensionSyncData::ExtensionSyncData(const Extension& extension,
                                     bool enabled,
                                     int disable_reasons,
                                     bool incognito_enabled,
                                     bool remote_install,
                                     const GURL& update_url)
    : ExtensionSyncData(extension,
                        enabled,
                        disable_reasons,
                        incognito_enabled,
                        remote_install,
                        update_url,
                        StringOrdinal(),
                        StringOrdinal(),
                        LAUNCH_TYPE_INVALID) {}

ExtensionSyncData::ExtensionSyncData(const Extension& extension,
                                     bool enabled,
                                     int disable_reasons,
                                     bool incognito_enabled,
                                     bool remote_install,
                                     const GURL& update_url,
                                     const StringOrdinal& app_launch_ordinal,
                                     const StringOrdinal& page_ordinal,
                                     LaunchType launch_type)
    : is_app_(extension.is_app()),
      id_(extension.id()),
      uninstalled_(false),
      enabled_(enabled),
      supports_disable_reasons_(true),
      disable_reasons_(disable_reasons),
      incognito_enabled_(incognito_enabled),
      remote_install_(remote_install),
      version_(extension.version()),
      update_url_(update_url),
      name_(extension.non_localized_name()),
      app_launch_ordinal_(app_launch_ordinal),
      page_ordinal_(page_ordinal),
      launch_type_(launch_type) {}

ExtensionSyncData::ExtensionSyncData(const ExtensionSyncData& other) = default;

ExtensionSyncData::~ExtensionSyncData() {}

// static
std::unique_ptr<ExtensionSyncData> ExtensionSyncData::CreateFromSyncData(
    const syncer::SyncData& sync_data) {
  std::unique_ptr<ExtensionSyncData> data(new ExtensionSyncData);
  if (data->PopulateFromSyncData(sync_data))
    return data;
  return nullptr;
}

// static
std::unique_ptr<ExtensionSyncData> ExtensionSyncData::CreateFromSyncChange(
    const syncer::SyncChange& sync_change) {
  std::unique_ptr<ExtensionSyncData> data(
      CreateFromSyncData(sync_change.sync_data()));
  if (!data.get())
    return nullptr;

  if (sync_change.change_type() == syncer::SyncChange::ACTION_DELETE)
    data->uninstalled_ = true;
  return data;
}

syncer::SyncData ExtensionSyncData::GetSyncData() const {
  sync_pb::EntitySpecifics specifics;
  if (is_app_)
    ToAppSpecifics(specifics.mutable_app());
  else
    ToExtensionSpecifics(specifics.mutable_extension());

  return syncer::SyncData::CreateLocalData(id_, name_, specifics);
}

syncer::SyncChange ExtensionSyncData::GetSyncChange(
    syncer::SyncChange::SyncChangeType change_type) const {
  return syncer::SyncChange(FROM_HERE, change_type, GetSyncData());
}

void ExtensionSyncData::ToExtensionSpecifics(
    sync_pb::ExtensionSpecifics* specifics) const {
  DCHECK(crx_file::id_util::IdIsValid(id_));
  specifics->set_id(id_);
  specifics->set_update_url(update_url_.spec());
  specifics->set_version(version_.GetString());
  specifics->set_enabled(enabled_);
  if (supports_disable_reasons_)
    specifics->set_disable_reasons(disable_reasons_);
  specifics->set_incognito_enabled(incognito_enabled_);
  specifics->set_remote_install(remote_install_);
}

void ExtensionSyncData::ToAppSpecifics(sync_pb::AppSpecifics* specifics) const {
  DCHECK(specifics);
  // Only sync the ordinal values and launch type if they are valid.
  if (app_launch_ordinal_.IsValid())
    specifics->set_app_launch_ordinal(app_launch_ordinal_.ToInternalValue());
  if (page_ordinal_.IsValid())
    specifics->set_page_ordinal(page_ordinal_.ToInternalValue());

  sync_pb::AppSpecifics::LaunchType sync_launch_type =
      static_cast<sync_pb::AppSpecifics::LaunchType>(launch_type_);

  // The corresponding validation of this value during processing of an
  // ExtensionSyncData is in ExtensionSyncService::ApplySyncData.
  if (launch_type_ >= LAUNCH_TYPE_FIRST && launch_type_ < NUM_LAUNCH_TYPES &&
      sync_pb::AppSpecifics_LaunchType_IsValid(sync_launch_type)) {
    specifics->set_launch_type(sync_launch_type);
  }

  for (const auto& linked_icon : linked_icons_) {
    sync_pb::LinkedAppIconInfo* linked_app_icon_info =
        specifics->add_linked_app_icons();
    DCHECK(linked_icon.url.is_valid());
    linked_app_icon_info->set_url(linked_icon.url.spec());
    linked_app_icon_info->set_size(linked_icon.size);
  }

  ToExtensionSpecifics(specifics->mutable_extension());
}

bool ExtensionSyncData::PopulateFromExtensionSpecifics(
    const sync_pb::ExtensionSpecifics& specifics) {
  if (!crx_file::id_util::IdIsValid(specifics.id())) {
    LOG(ERROR) << "Attempt to sync bad ExtensionSpecifics (bad ID):\n"
               << GetExtensionSpecificsLogMessage(specifics);
    RecordBadSyncData(BAD_EXTENSION_ID);
    return false;
  }

  base::Version specifics_version(specifics.version());
  if (!specifics_version.IsValid()) {
    LOG(ERROR) << "Attempt to sync bad ExtensionSpecifics (bad version):\n"
               << GetExtensionSpecificsLogMessage(specifics);
    RecordBadSyncData(BAD_VERSION);
    return false;
  }

  // The update URL must be either empty or valid.
  GURL specifics_update_url(specifics.update_url());
  if (!specifics_update_url.is_empty() && !specifics_update_url.is_valid()) {
    LOG(ERROR) << "Attempt to sync bad ExtensionSpecifics (bad update URL):\n"
               << GetExtensionSpecificsLogMessage(specifics);
    RecordBadSyncData(BAD_UPDATE_URL);
    return false;
  }

  id_ = specifics.id();
  update_url_ = specifics_update_url;
  version_ = specifics_version;
  enabled_ = specifics.enabled();
  supports_disable_reasons_ = specifics.has_disable_reasons();
  disable_reasons_ = specifics.disable_reasons();
  incognito_enabled_ = specifics.incognito_enabled();
  remote_install_ = specifics.remote_install();
  return true;
}

bool ExtensionSyncData::PopulateFromAppSpecifics(
    const sync_pb::AppSpecifics& specifics) {
  if (!PopulateFromExtensionSpecifics(specifics.extension()))
    return false;

  is_app_ = true;

  app_launch_ordinal_ = syncer::StringOrdinal(specifics.app_launch_ordinal());
  page_ordinal_ = syncer::StringOrdinal(specifics.page_ordinal());

  launch_type_ = specifics.has_launch_type()
                     ? static_cast<LaunchType>(specifics.launch_type())
                     : LAUNCH_TYPE_INVALID;

  // Bookmark apps and chrome apps both have "app" specifics, but only bookmark
  // apps filled out the `bookmark_app*` fields.
  is_deprecated_bookmark_app_ = specifics.has_bookmark_app_url();

  for (int i = 0; i < specifics.linked_app_icons_size(); ++i) {
    const sync_pb::LinkedAppIconInfo& linked_app_icon_info =
        specifics.linked_app_icons(i);
    if (linked_app_icon_info.has_url() && linked_app_icon_info.has_size()) {
      LinkedAppIconInfo linked_icon;
      linked_icon.url = GURL(linked_app_icon_info.url());
      linked_icon.size = linked_app_icon_info.size();
      linked_icons_.push_back(linked_icon);
    }
  }

  return true;
}

bool ExtensionSyncData::PopulateFromSyncData(
    const syncer::SyncData& sync_data) {
  const sync_pb::EntitySpecifics& entity_specifics = sync_data.GetSpecifics();

  if (entity_specifics.has_app())
    return PopulateFromAppSpecifics(entity_specifics.app());

  if (entity_specifics.has_extension())
    return PopulateFromExtensionSpecifics(entity_specifics.extension());

  LOG(ERROR) << "Attempt to sync bad EntitySpecifics: no extension data.";
  RecordBadSyncData(NO_EXTENSION_SPECIFICS);
  return false;
}

}  // namespace extensions
