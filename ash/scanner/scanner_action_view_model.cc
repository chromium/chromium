// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_view_model.h"

#include <string>
#include <utility>
#include <variant>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scanner/scanner_action_handler.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

ScannerActionViewModel::ScannerActionViewModel(
    ScannerAction action,
    base::WeakPtr<ScannerCommandDelegate> delegate)
    : action_(std::move(action)), delegate_(std::move(delegate)) {}

ScannerActionViewModel::ScannerActionViewModel(const ScannerActionViewModel&) =
    default;

ScannerActionViewModel& ScannerActionViewModel::operator=(
    const ScannerActionViewModel&) = default;

ScannerActionViewModel::ScannerActionViewModel(ScannerActionViewModel&&) =
    default;

ScannerActionViewModel& ScannerActionViewModel::operator=(
    ScannerActionViewModel&&) = default;

ScannerActionViewModel::~ScannerActionViewModel() = default;

std::u16string ScannerActionViewModel::GetText() const {
  // TODO(b/369470078): Replace this with finalised translated strings.
  return std::visit(
      base::Overloaded{
          [](const manta::proto::NewEventAction&) { return u"New event"; },
          [](const manta::proto::NewContactAction&) { return u"New contact"; },
          [](const manta::proto::NewGoogleDocAction&) {
            return u"New Google Doc";
          },
          [](const manta::proto::NewGoogleSheetAction&) {
            return u"New Google Sheet";
          },
          [](const manta::proto::CopyToClipboardAction&) {
            return u"Copy to clipboard";
          },
      },
      action_);
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
