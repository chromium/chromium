// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/scanner_action_provider.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "url/gurl.h"

namespace {

ash::ScannerAction CreateOpenUrlAction(const GURL& url) {
  return ash::OpenUrlAction{url};
}

}  // namespace

ScannerActionProvider::ScannerActionProvider() = default;

ScannerActionProvider::~ScannerActionProvider() = default;

void ScannerActionProvider::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    OnActionsResolved callback) {
  // TODO(b/363100868): Fetch available actions from service
  std::move(callback).Run(base::ok(std::vector<ash::ScannerAction>{
      CreateOpenUrlAction(GURL("https://www.google.com")),
  }));
}
