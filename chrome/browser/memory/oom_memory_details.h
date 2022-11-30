// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_OOM_MEMORY_DETAILS_H_
#define CHROME_BROWSER_MEMORY_OOM_MEMORY_DETAILS_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/memory_details.h"

namespace memory {

////////////////////////////////////////////////////////////////////////////////
// OomMemoryDetails logs details about all Chrome processes during an out-of-
// memory event in an attempt to identify the culprit.
class OomMemoryDetails : public MemoryDetails {
 public:
  OomMemoryDetails(const OomMemoryDetails&) = delete;
  OomMemoryDetails& operator=(const OomMemoryDetails&) = delete;

  // Logs the memory details asynchronously.
  // The |title| is printed at the beginning of the message.
  static void Log(const std::string& title);

 private:
  OomMemoryDetails(const std::string& title);
  ~OomMemoryDetails() override;

  // MemoryDetails overrides:
  void OnDetailsAvailable() override;

  std::string title_;
  base::TimeTicks start_time_;
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_OOM_MEMORY_DETAILS_H_
