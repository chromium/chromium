// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_PACKET_METADATA_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_PACKET_METADATA_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/policy/core/browser/webui/json_generation.h"

// SupportPacketMetadata stores the metadata related to the given support packet
// and writes these metadata into "metadata" file. The information that will be
// included in metadata file:
// - Timestamp of data collection
// - Support Case ID
// - Issue Description
// - Email Address (optional)
// - GUID of the support packet: This ID matches with the `upload_id` field on
// server.
// - List of data collectors
// - Chrome details:
//   - Platform and OS (contains board and channel for ChromeOS)
//   - Revision
//   - Version
// - Serial number (machine ID) (only on ChromeOS)
// - Errors if any happened during Support Tool data collection
class SupportPacketMetadata {
 public:
  SupportPacketMetadata(std::string case_id,
                        std::string email_address,
                        std::string issue_description,
                        std::optional<std::string> upload_id);

  ~SupportPacketMetadata();

  // Adds timestamp of data collection and included data collector names to
  // metadata. On ChromeOS, adds the serial number of the device to metadata.
  // Then runs `on_metadata_contents_populated`.
  void PopulateMetadataContents(
      const base::Time& timestamp,
      const std::vector<std::unique_ptr<DataCollector>>&
          data_collectors_included,
      base::OnceClosure on_metadata_contents_populated);

  // Returns the case ID that's attached to the support packet. Empty if there's
  // no case ID attached.
  const std::string& GetCaseId();

  // Returns the PII sensitive data that's included in the metadata file.
  const PIIMap& GetPII();

  // Adds the error messages to "Support Tool Errors" field of metadata.
  // `errors` will be added to `error_messages_` and it will be added to
  // `metadata_` only when WriteMetadataFile() is called hence this function can
  // be called multiple times to append more errors to metadata file.
  void InsertErrors(const std::set<SupportToolError>& errors);

  // Creates "metadata.txt" file in `target_path` and writes the metadata.
  // Removes all PII sensitive data from metadata except the PII types in
  // `pii_to_keep`. Runs `on_metadata_file_written` when file is written.
  void WriteMetadataFile(base::FilePath target_path,
                         std::set<redaction::PIIType> pii_to_keep,
                         base::OnceClosure on_matadata_file_written);

 private:
  // Returns a GUID (universally unique identifier) for the support packet.
  std::string GetGUIDForSupportPacket();

  // Populates the fields that contain Chrome metadata in `metadata_`. Those
  // fields are Platform/OS, Chrome revision and Chrome version.
  void SetChromeMetadataFields();

  // Looks-up the string with key `chrome_metadata_key` in `chrome_metadata` and
  // if found, adds it to `metadata_` as value with key `support_packet_key`.
  void FindStringAndSetInSupportPacketMetadata(
      const base::Value::Dict& chrome_metadata,
      const char* chrome_metadata_key,
      const char* support_packet_key);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Is called when machine statistics is loaded. Puts `machine_serial` on
  // `metadata_` and runs `on_metadata_contents_populated`.
  void OnMachineStatisticsLoaded(
      base::OnceClosure on_metadata_contents_populated);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Returns the string of `timestamp` in a numeric date and time with time zone
  // such as "12/13/52 2:44:30 PM PST".
  std::string GetTimestampString(const base::Time& timestamp);

  // Returns a string that contains the list of name of data collectors
  // included.
  std::string GetDataCollectorsListString(
      const std::vector<std::unique_ptr<DataCollector>>&
          data_collectors_included);

  // Adds support tool error messages to `metadata_`.
  void AddErrorMessagesToMetadata();

  std::map<std::string, std::string> metadata_;
  std::vector<std::string> error_messages_;
  PIIMap pii_;
  base::WeakPtrFactory<SupportPacketMetadata> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_PACKET_METADATA_H_
