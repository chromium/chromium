// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_HANDLER_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_HANDLER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/support_tool/data_collector.h"

using SupportToolDataCollectedCallback =
    base::OnceCallback<void(const PIIMap&)>;

// The SupportToolHandler collects debug data from a list of DataCollectors.
//
// EXAMPLE:
// class Foo {
//  public:
//   void ProcessCollectedData(const PIIMap& detected) {
//     // do something with the detected PII.
//   }
//   void GetSupportData() {
//     SupportToolHandler* handler = new SupportToolHandler();
//     handler->AddSource(std::make_unique<DataCollectorOne>());
//     handler->AddSource(std::make_unique<DataCollectorTwo>());
//     handler->CollectSupportData(
//         base::BindOnce(&Foo::ProcessCollectedData, this));
//   }
// };

class SupportToolHandler {
 public:
  SupportToolHandler();
  ~SupportToolHandler();

  // Adds `collector` to the list of DataCollectors the SupportToolHandler
  // will collect data from.
  void AddDataCollector(std::unique_ptr<DataCollector> collector);

  // Collects data from the DataCollectors added to the handler.
  void CollectSupportData(
      SupportToolDataCollectedCallback on_data_collection_done_callback);

 private:
  // OnDataCollected is called when a single DataCollector finished collecting
  // data. Runs `barrier_closure` to make the handler wait until all
  // DataCollectors finish collecting.
  void OnDataCollected(base::RepeatingClosure barrier_closure);

  // OnAllDataCollected is called by a BarrierClosure when all DataCollectors
  // finish collecting data. Returns the detected PII by running
  // `on_data_collection_done_callback_`.
  void OnAllDataCollected();

  SEQUENCE_CHECKER(sequence_checker_);
  PIIMap detected_pii_;
  std::vector<std::unique_ptr<DataCollector>> data_collectors_;
  SupportToolDataCollectedCallback on_data_collection_done_callback_;
  base::WeakPtrFactory<SupportToolHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_HANDLER_H_
