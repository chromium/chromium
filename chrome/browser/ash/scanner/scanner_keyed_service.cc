// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/scanner_keyed_service.h"

#include "ash/public/cpp/scanner/scanner_action.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"

ScannerKeyedService::ScannerKeyedService(Profile* profile) {}

ScannerKeyedService::~ScannerKeyedService() = default;

ash::ScannerSystemState ScannerKeyedService::GetSystemState() const {
  return system_state_provider_.GetSystemState();
}

void ScannerKeyedService::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    base::OnceCallback<void(ash::ScannerActionsResponse)> callback) {
  action_provider_.FetchActionsForImage(jpeg_bytes, std::move(callback));
}

void ScannerKeyedService::Shutdown() {}
