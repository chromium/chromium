// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_view_model.h"

#include <string>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "components/manta/proto/scanner.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

ScannerActionViewModel::ScannerActionViewModel(
    manta::proto::ScannerAction unpopulated_action,
    scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes)
    : unpopulated_action_(std::move(unpopulated_action)),
      downscaled_jpeg_bytes_(std::move(downscaled_jpeg_bytes)) {}

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
  switch (unpopulated_action_.action_case()) {
    case manta::proto::ScannerAction::kNewEvent:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_CREATE_EVENT_LABEL);
    case manta::proto::ScannerAction::kNewContact:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_CREATE_CONTACT_LABEL);
    case manta::proto::ScannerAction::kNewGoogleDoc:
      return l10n_util::GetStringUTF16(IDS_ASH_SCANNER_ACTION_CREATE_DOC);
    case manta::proto::ScannerAction::kNewGoogleSheet:
      return l10n_util::GetStringUTF16(IDS_ASH_SCANNER_ACTION_CREATE_SHEET);
    case manta::proto::ScannerAction::kCopyToClipboard:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SCANNER_ACTION_COPY_TEXT_AND_FORMAT);
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      // This should only be possible if `unpopulated_action_` has been
      // previously moved.
      NOTREACHED();
  }

  // This should not be possible as all Protobuf variant case enums should
  // always be known.
  NOTREACHED();
}

const gfx::VectorIcon& ScannerActionViewModel::GetIcon() const {
  switch (unpopulated_action_.action_case()) {
    case manta::proto::ScannerAction::kNewEvent:
      return kScannerCalendarIcon;
    case manta::proto::ScannerAction::kNewContact:
      return kScannerNewContactIcon;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      return kScannerDocIcon;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      return kScannerSheetIcon;
    case manta::proto::ScannerAction::kCopyToClipboard:
      return kScannerClipboardIcon;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      // This should only be possible if `unpopulated_action_` has been
      // previously moved.
      NOTREACHED();
  }
}

manta::proto::ScannerAction::ActionCase ScannerActionViewModel::GetActionCase()
    const {
  return unpopulated_action_.action_case();
}

}  // namespace ash
