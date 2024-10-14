// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_view_model.h"

#include <string>
#include <utility>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scanner/scanner_action_handler.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace ash {

ScannerActionViewModel::ScannerActionViewModel(ScannerAction action)
    : action_(std::move(action)) {}

ScannerActionViewModel::ScannerActionViewModel(const ScannerActionViewModel&) =
    default;

ScannerActionViewModel& ScannerActionViewModel::operator=(
    const ScannerActionViewModel&) = default;

ScannerActionViewModel::~ScannerActionViewModel() = default;

std::u16string ScannerActionViewModel::GetText() const {
  // TODO(b/369470078): Replace this placeholder.
  return u"Placeholder action";
}

const gfx::VectorIcon& ScannerActionViewModel::GetIcon() const {
  // TODO(b/369470078): Replace this placeholder.
  return kCaptureModeIcon;
}

base::OnceClosure ScannerActionViewModel::ToCallback(
    ScannerActionViewModel::ActionFinishedCallback
        action_finished_callback) && {
  return base::BindOnce(&HandleScannerAction, std::move(action_),
                        std::move(action_finished_callback));
}

}  // namespace ash
