// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_handler.h"

#include <string_view>
#include <variant>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "url/gurl.h"

namespace ash {

namespace {

void OpenInBrowserTab(const GURL& gurl) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      gurl, NewWindowDelegate::OpenUrlFrom::kUnspecified,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

}  // namespace

void HandleScannerAction(const ScannerAction& action,
                         base::OnceCallback<void(bool)> callback) {
  std::visit(
      base::Overloaded{
          [&](const OpenUrlCommand& command) {
            OpenInBrowserTab(command.url);
            std::move(callback).Run(true);
          },
      },
      action.command);
}

}  // namespace ash
