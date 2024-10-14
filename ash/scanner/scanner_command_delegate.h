// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_COMMAND_DELEGATE_H_
#define ASH_SCANNER_SCANNER_COMMAND_DELEGATE_H_

#include "ash/ash_export.h"

class GURL;

namespace ash {

// Delegate for `HandleScannerAction` to access its dependencies for performing
// the command.
class ASH_EXPORT ScannerCommandDelegate {
 public:
  virtual ~ScannerCommandDelegate();

  // Opens the provided URL in the browser.
  virtual void OpenUrl(const GURL& url) = 0;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_COMMAND_DELEGATE_H_
