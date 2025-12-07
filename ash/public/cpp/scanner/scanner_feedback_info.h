// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_FEEDBACK_INFO_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_FEEDBACK_INFO_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

struct ASH_PUBLIC_EXPORT ScannerFeedbackInfo {
  std::string action_details;
  // If nullptr, no image is shown in the feedback form.
  scoped_refptr<base::RefCountedMemory> screenshot;

  ScannerFeedbackInfo(std::string action_details,
                      scoped_refptr<base::RefCountedMemory> screenshot);

  ScannerFeedbackInfo(const ScannerFeedbackInfo&);
  ScannerFeedbackInfo& operator=(const ScannerFeedbackInfo&);
  ScannerFeedbackInfo(ScannerFeedbackInfo&&);
  ScannerFeedbackInfo& operator=(ScannerFeedbackInfo&&);

  ~ScannerFeedbackInfo();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_FEEDBACK_INFO_H_
