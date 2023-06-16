// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_MANAGEMENT_BACKEND_PRINT_MANAGEMENT_HANDLER_H_
#define ASH_WEBUI_PRINT_MANAGEMENT_BACKEND_PRINT_MANAGEMENT_HANDLER_H_

namespace ash::printing::printing_manager {

class PrintManagementHandler {
 public:
  PrintManagementHandler();
  PrintManagementHandler(const PrintManagementHandler&) = delete;
  PrintManagementHandler& operator=(const PrintManagementHandler&) = delete;
  ~PrintManagementHandler();
};

}  // namespace ash::printing::printing_manager

#endif  // ASH_WEBUI_PRINT_MANAGEMENT_BACKEND_PRINT_MANAGEMENT_HANDLER_H_
