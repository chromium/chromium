// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/scanner_action_provider.h"

#include <string_view>
#include <vector>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "url/gurl.h"

namespace {

constexpr std::string_view kDisplayName = "Open Search";

ash::ScannerAction CreateOpenUrlAction(const GURL& url) {
  return ash::ScannerAction(kDisplayName, ash::OpenUrlCommand{url});
}

}  // namespace

ScannerActionProvider::ScannerActionProvider() = default;

ScannerActionProvider::~ScannerActionProvider() = default;

void ScannerActionProvider::FetchActions(OnActionsResolved callback) {
  // TODO(b/363100868): Fetch available actions from service
  std::move(callback).Run(base::ok(std::vector<ash::ScannerAction>{
      CreateOpenUrlAction(GURL("https://www.google.com")),
  }));
}
