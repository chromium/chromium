// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_util.h"

#include <memory>

#include "chrome/browser/support_tool/support_tool_handler.h"
#include "chrome/browser/support_tool/ui_hierarchy_data_collector.h"

std::unique_ptr<SupportToolHandler> GetSupportToolHandler(bool chrome_os,
                                                          bool chrome_browser) {
  std::unique_ptr<SupportToolHandler> handler =
      std::make_unique<SupportToolHandler>();
  if (chrome_os) {
    handler->AddDataCollector(std::make_unique<UiHierarchyDataCollector>());
  }
  return handler;
}
