// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_view_model.h"

#include <string>
#include <utility>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scanner/scanner_action_handler.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace ash {

ScannerActionViewModel::ScannerActionViewModel(
    ScannerAction action,
    base::WeakPtr<ScannerCommandDelegate> delegate)
    : action_(std::move(action)), delegate_(std::move(delegate)) {}

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
    ScannerCommandCallback action_finished_callback) && {
  return base::BindOnce(&HandleScannerCommand, std::move(delegate_),
                        ScannerActionToCommand(std::move(action_)),
                        std::move(action_finished_callback));
}

}  // namespace ash
