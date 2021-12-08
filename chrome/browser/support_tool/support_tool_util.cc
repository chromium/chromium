// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_util.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/support_tool/ash/ui_hierarchy_data_collector.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<SupportToolHandler> GetSupportToolHandler(bool chrome_os,
                                                          bool chrome_browser) {
  std::unique_ptr<SupportToolHandler> handler =
      std::make_unique<SupportToolHandler>();
  if (chrome_os) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    handler->AddDataCollector(std::make_unique<UiHierarchyDataCollector>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
  return handler;
}
