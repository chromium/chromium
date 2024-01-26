// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SOURCE_DESTINATION_MATCHER_ASH_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SOURCE_DESTINATION_MATCHER_ASH_H_

#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/values.h"

namespace base {
template <typename T>
class WeakPtr;
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace storage {
class FileSystemURL;
}

namespace file_manager {
class Volume;
}

namespace enterprise_connectors {

// `SourceDestinationMatcherAsh` allows to match sources and destinations on the
// chrome storage file system with different file system types.
// A user should first call `AddFilters()` with an appropriate `settings_list`
// one or more times.
// The function `Match()` can then be used to find all corresponding matches
// based on the passed settings.
// The format of a valid `settings_list` parameter is described in the file
// components/policy/resources/policy_templates.json.
//
//    [
//      {
//        "sources": [
//          {"file_system_type":"ANY"}
//        ],
//        "destinations": [
//          {"file_system_type":"ANY"}
//        ]
//      },
//      {
//        "sources": [
//          {"file_system_type":"MY_FILES"},
//          {"file_system_type":"ARC"},
//        ],
//        "destinations": [
//          {"file_system_type":"REMOVABLE"}
//        ]
//      }
//    ]
//
// Every FileSystemURL pair would then match the first rule.
// For the second rule, only FileSystemURLs would match if the `source_url` lies
// in a file system associated with MY_FILES or ARC and the `destination_url`
// lies in a REMOVABLE file system.
//
// Note that if you pass the above `settings_list` using the `AddFilters()`
// function with *id==start_id, for the first rule `start_id+1` and for the
// second rule `start_id+2` are returned.
class SourceDestinationMatcherAsh {
 public:
  using ID = size_t;

  SourceDestinationMatcherAsh();

  ~SourceDestinationMatcherAsh();

  void AddFilters(ID* id, const base::Value::List* settings_list);

  std::set<ID> Match(content::BrowserContext* context,
                     const storage::FileSystemURL& source_url,
                     const storage::FileSystemURL& destination_url) const;

  // Returns a descriptive string of the volume associated with `path`.
  static std::string GetVolumeDescriptionFromPath(
      content::BrowserContext* context,
      const base::FilePath& path);

 private:
  // This enum mostly corresponds to the values in file_manager::VolumeType.
  // For file_manager::VOLUME_TYPE_GUEST_OS, the guest_os::VmType is
  // additionally kept into account.
  // This enum should be kept in sync with the above enums.
  // One FsType can be corresponding to multiple combinations of
  // file_manager::VolumeType and guest_os::VmType.
  // E.g., kArc corresponds to:
  // (file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::ARCVM),
  // file_manager::VOLUME_TYPE_MEDIA_VIEW and
  // file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER.
  //
  // When adding a value here, the policy in policy_templates.json should also
  // be updated.
  enum class FsType {
    kTesting = -1,        // Test file systems.
    kUnknown = 0,         // Matches unknown file system types.
    kAny = 1,             // Matches any file system, including unknowns.
    kMyFiles,             // Also includes downloads directory.
    kRemovable,           // USB sticks, etc.
    kDeviceMediaStorage,  // MTP, PTP, ...
    kProvided,            // Provided through FileSystemProvider API.
    kArc,                 // Includes ArcPlayFiles, ArcDocumentsProvider, ...
    kGoogleDrive,
    kSmb,
    kCrostini,    // Linux VM.
    kPluginVm,    // Parallels.
    kBorealis,    // Steam on CrOS.
    kBruschetta,  // Custom Linux VMs.
    kUnknownVm,
  };

  struct SourceDestinationEntry {
    SourceDestinationEntry();
    SourceDestinationEntry(const SourceDestinationEntry& other);
    SourceDestinationEntry(SourceDestinationEntry&& other);
    ~SourceDestinationEntry();

    bool Matches(FsType source_type, FsType destination_type) const;

    ID id;
    std::set<FsType> fs_sources;
    std::set<FsType> fs_destinations;
    // TODO(crbug.com/1340553): Add support for app ids and smb server
    // configurations.
  };

  static FsType VolumeToFsType(base::WeakPtr<file_manager::Volume> volume);
  static FsType PathToFsType(content::BrowserContext* context,
                             const base::FilePath& path);

  static std::optional<FsType> StringToFsType(const std::string& s);
  std::set<FsType> ValueListToFsTypes(
      const base::Value::List* source_or_destination_list);

  static std::string FsTypeToString(FsType fs_type);

  std::vector<SourceDestinationEntry> source_destination_entries_;

  FRIEND_TEST_ALL_PREFIXES(
      SourceDestinationMatcherAshFsTypeStringConversionTest,
      StringToType);
  FRIEND_TEST_ALL_PREFIXES(
      SourceDestinationMatcherAshFsTypeStringConversionTest,
      TypeToString);
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SOURCE_DESTINATION_MATCHER_ASH_H_
