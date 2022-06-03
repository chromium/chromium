// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_handler.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "chrome/browser/support_tool/data_collector.h"

SupportToolHandler::SupportToolHandler() = default;
SupportToolHandler::~SupportToolHandler() = default;

void SupportToolHandler::AddDataCollector(
    std::unique_ptr<DataCollector> collector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(collector);
  data_collectors_.emplace_back(std::move(collector));
}

void SupportToolHandler::CollectSupportData(
    SupportToolDataCollectedCallback on_data_collection_done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!on_data_collection_done_callback.is_null());
  DCHECK(!data_collectors_.empty());

  on_data_collection_done_callback_ =
      std::move(on_data_collection_done_callback);

  base::RepeatingClosure collect_data_barrier_closure = base::BarrierClosure(
      data_collectors_.size(),
      base::BindOnce(&SupportToolHandler::OnAllDataCollected,
                     weak_ptr_factory_.GetWeakPtr()));

  for (auto& data_collector : data_collectors_) {
    data_collector->CollectDataAndDetectPII(base::BindOnce(
        &SupportToolHandler::OnDataCollected, weak_ptr_factory_.GetWeakPtr(),
        collect_data_barrier_closure));
  }
}

void SupportToolHandler::OnDataCollected(
    base::RepeatingClosure barrier_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(barrier_closure).Run();
}

void SupportToolHandler::OnAllDataCollected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& data_collector : data_collectors_) {
    const PIIMap& collected = data_collector->GetDetectedPII();
    // Use std::multipmap.merge() function after migration to C++17.
    detected_pii_.insert(collected.begin(), collected.end());
  }
  std::move(on_data_collection_done_callback_).Run(detected_pii_);
}
