// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_KEYWORD_INPUT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_KEYWORD_INPUT_H_

#include <string>

namespace app_list {

// This enum represents which type of System Info will be displayed.
enum class SystemInfoInputType { kCPU, kVersion, kMemory, kBattery, kStorage };

struct SystemInfoKeywordInput {
  SystemInfoKeywordInput() = default;
  SystemInfoKeywordInput(SystemInfoInputType input_type,
                         std::u16string keyword);

  ~SystemInfoKeywordInput() = default;

  SystemInfoInputType GetInputType();
  std::u16string GetKeyword();

 private:
  SystemInfoInputType input_type_;
  std::u16string keyword_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_KEYWORD_INPUT_H_
