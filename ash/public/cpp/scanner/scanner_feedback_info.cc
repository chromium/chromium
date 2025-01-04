// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/scanner/scanner_feedback_info.h"

#include <string>
#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

ScannerFeedbackInfo::ScannerFeedbackInfo(
    std::string action_details,
    scoped_refptr<base::RefCountedMemory> screenshot)
    : action_details(std::move(action_details)),
      screenshot(std::move(screenshot)) {}

ScannerFeedbackInfo::ScannerFeedbackInfo(const ScannerFeedbackInfo&) = default;
ScannerFeedbackInfo& ScannerFeedbackInfo::operator=(
    const ScannerFeedbackInfo&) = default;
ScannerFeedbackInfo::ScannerFeedbackInfo(ScannerFeedbackInfo&&) = default;
ScannerFeedbackInfo& ScannerFeedbackInfo::operator=(ScannerFeedbackInfo&&) =
    default;

ScannerFeedbackInfo::~ScannerFeedbackInfo() = default;

}  // namespace ash
