// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_HANDLER_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_HANDLER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_packet_metadata.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_source.h"

using SupportToolDataCollectedCallback =
    base::OnceCallback<void(const PIIMap&, std::set<SupportToolError>)>;

using SupportToolDataExportedCallback =
    base::OnceCallback<void(base::FilePath, std::set<SupportToolError>)>;

// The SupportToolHandler collects debug data from a list of DataCollectors.
//
// EXAMPLE:
// class Foo {
//  public:
//  void ProcessCollectedData(const PIIMap& detected,
//                           std::set<SupportToolError> errors)
//                           {
//     // Do something with the detected PII.
//     // Check if error is returned.
//     if(!errors.empty()) {
//       // do something with the error.
//     }
//   }
//   void GetSupportData() {
//     handler_.AddSource(std::make_unique<DataCollectorOne>());
//     handler_.AddSource(std::make_unique<DataCollectorTwo>());
//     handler_.CollectSupportData(base::BindOnce(&Foo::ProcessCollectedData,
//                                           weak_ptr_factory_.GetWeakPtr()));
//   }
//   void OnDataExported(base::FilePath path, std::set<SupportToolError> errors)
//   {
//     // Do something about the data that has been exported.
//     // Check and do something if any errors returned.
//   }
//   void ExportSupportData() {
//     std::set<PIIMap> pii_to_keep;
//     // Add some PIITypes to keep into the pii_to_keep set.
//     base::FilePath target_path{"directory_to_export_data"};
//     handler_.ExportCollectedData(pii_to_keep,
//                             target_path,
//                             base::BindOnce(&Foo::OnDataExported,
//                                            weak_ptr_factory_.GetWeakPtr()));
//   }
//  private:
//   // The class has a SupportToolHandler member.
//   SupportToolHandler handler_;
//   base::WeakPtrFactory<Foo> weak_ptr_factory_{this};
// };

class SupportToolHandler {
 public:
  // Intended to be used for unit tests. Initializes `case_id_`,
  // `email_address_` and `issue_description_` as empty string.
  SupportToolHandler();
  SupportToolHandler(std::string case_id,
                     std::string email_address,
                     std::string issue_description,
                     std::optional<std::string> upload_id);
  ~SupportToolHandler();

  // Returns the support case ID.
  const std::string& GetCaseId();

  // Returns the timestamp of data collection start. Must be called after
  // CollectSupportData() has been called.
  const base::Time& GetDataCollectionTimestamp();

  // Adds `collector` to the list of DataCollectors the SupportToolHandler
  // will collect data from.
  void AddDataCollector(std::unique_ptr<DataCollector> collector);

  // Collects data from the DataCollectors added to the handler. This function
  // should be called only once on an instance of SupportToolHandler.
  void CollectSupportData(
      SupportToolDataCollectedCallback on_data_collection_done_callback);

  // Exports collected data to the `target_path` and archives the file. Runs
  // `on_data_exported_callback` with the set of errors in case of an error and
  // the exported filepath. The filepath given to callback will be empty if the
  // export couldn't happen due to an error. This function should be called only
  // once on an instance of SupportToolHandler.
  void ExportCollectedData(
      std::set<redaction::PIIType> pii_types_to_keep,
      base::FilePath target_path,
      SupportToolDataExportedCallback on_data_exported_callback);

  // Returns reference to `data_collectors_` for testing.
  const std::vector<std::unique_ptr<DataCollector>>&
  GetDataCollectorsForTesting();

 private:
  // OnDataCollected is called when a single DataCollector finished collecting
  // data. Runs `barrier_closure` to make the handler wait until all
  // DataCollectors finish collecting.
  void OnDataCollected(base::RepeatingClosure barrier_closure,
                       std::optional<SupportToolError> error);

  // OnAllDataCollected is called by a BarrierClosure when all DataCollectors
  // finish collecting data. Returns the detected PII by running
  // `on_data_collection_done_callback_`.
  void OnAllDataCollected();

  void OnMetadataContentsPopulated();

  // Adds the contents of `pii_map` to `detected_pii_` of this instance.
  void AddDetectedPII(const PIIMap& pii_map);

  // Exports collected data into the `tmp_path`. Creates a path for each
  // DataCollector with their name. The DataCollectors will export their output
  // to that path then the contents of the `tmp_path` will be put inside a zip
  // archive on `target_path`.
  void ExportIntoTempDir(std::set<redaction::PIIType> pii_types_to_keep,
                         base::FilePath target_path,
                         base::FilePath tmp_path);

  // OnDataCollectorDoneExporting is called when a single DataCollector finished
  // exporting data. Runs `barrier_closure` to make the handler wait until all
  // DataCollectors finish collecting.
  void OnDataCollectorDoneExporting(base::RepeatingClosure barrier_closure,
                                    std::optional<SupportToolError> error);

  // OnAllDataCollectorsDoneExporting is called by a BarrierClosure when all
  // DataCollectors finish exporting data to their given filepaths. Calls
  // `metadata_` to add the metadata file to `tmp_path`.
  void OnAllDataCollectorsDoneExporting(
      base::FilePath tmp_path,
      base::FilePath target_path,
      std::set<redaction::PIIType> pii_types_to_keep);

  // OnMetadataFileWritten is called when metadata file is written. Archives
  // the data exported by DataCollectors inside a .zip archive and calls
  // OnDataExportDone().
  void OnMetadataFileWritten(base::FilePath tmp_path,
                             base::FilePath target_path);

  // Cleans up the temporary directory created to store the output files and
  // then calls `on_data_export_done_callback_`.
  void OnDataExportDone(base::FilePath exported_path);

  // Cleans up `this.temp_dir_`. We need to clean-up the temporary directory
  // explicitly since SupportToolHandler will work on UI thread and all file
  // operation must be done in a worker thread to not block UI thread. We can
  // only pass the file path between threads as sharing the ScopedTempDir object
  // is not safe to pass between threads.
  void CleanUp();

  SEQUENCE_CHECKER(sequence_checker_);
  base::Time data_collection_timestamp_;
  SupportPacketMetadata metadata_;
  PIIMap detected_pii_;
  std::vector<std::unique_ptr<DataCollector>> data_collectors_;
  // Stores the set of errors that are returned from DataCollector calls. Reset
  // the set each time SupportToolHandler starts executing a function.
  std::set<SupportToolError> collected_errors_;
  SupportToolDataCollectedCallback on_data_collection_done_callback_;
  SupportToolDataExportedCallback on_data_export_done_callback_;
  // Temporary directory for storing the output files. Will be deleted when the
  // data export is done or on destruction of the SupportToolHandler instance if
  // it hasn't been removed before.
  base::FilePath temp_dir_;
  // SequencedTaskRunner and RedactionToolContainer for the data collectors that
  // will need to use redaction::RedactionTool for masking PII from the
  // collected logs.
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
  base::WeakPtrFactory<SupportToolHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_HANDLER_H_
