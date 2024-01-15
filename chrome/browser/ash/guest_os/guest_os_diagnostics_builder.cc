// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_diagnostics_builder.h"

#include <utility>

#include "base/check_op.h"
#include "chrome/browser/ash/guest_os/guest_os_diagnostics.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace guest_os {

namespace {
using Status = mojom::DiagnosticEntry::Status;
}  // namespace

DiagnosticsBuilder::DiagnosticsBuilder() {
  diagnostics_ = mojom::Diagnostics::New();
}

DiagnosticsBuilder::~DiagnosticsBuilder() = default;

mojom::DiagnosticsPtr DiagnosticsBuilder::Build() {
  return std::move(diagnostics_);
}

DiagnosticsBuilder::EntryBuilder::EntryBuilder(const std::string& requirement)
    : entry_{mojom::DiagnosticEntry::New(requirement,
                                         Status::kPass,
                                         /*explanation=*/nullptr)} {}
DiagnosticsBuilder::EntryBuilder::EntryBuilder(int requirement_message_id)
    : EntryBuilder(l10n_util::GetStringUTF8(requirement_message_id)) {}
DiagnosticsBuilder::EntryBuilder::EntryBuilder(EntryBuilder&&) = default;
DiagnosticsBuilder::EntryBuilder::~EntryBuilder() = default;

void DiagnosticsBuilder::EntryBuilder::SetNotApplicable() {
  DCHECK_EQ(entry_->status, Status::kPass)
      << "SetNotApplicable() should only be called on a builder in the initial "
         "state";
  entry_->status = Status::kNotApplicable;
}

DiagnosticsBuilder::EntryBuilder& DiagnosticsBuilder::EntryBuilder::SetFail(
    const std::string& explanation,
    const std::optional<GURL>& learn_more_link) {
  DCHECK_EQ(entry_->status, Status::kPass)
      << "SetFail() should only be called on a builder in the initial state";

  entry_->status = Status::kFail;
  entry_->explanation =
      mojom::DiagnosticMessage::New(explanation, learn_more_link);

  return *this;
}

void DiagnosticsBuilder::EntryBuilder::OverrideTopError(
    const std::string& error,
    const std::optional<GURL>& learn_more_link) {
  DCHECK_EQ(entry_->status, Status::kFail);

  overridden_top_error_ = mojom::DiagnosticMessage::New(error, learn_more_link);
}

DiagnosticsBuilder::EntryBuilder& DiagnosticsBuilder::EntryBuilder::SetFail(
    int explanation_message_id,
    const std::optional<GURL>& learn_more_link) {
  return SetFail(l10n_util::GetStringUTF8(explanation_message_id),
                 learn_more_link);
}

void DiagnosticsBuilder::EntryBuilder::OverrideTopError(
    int error_message_id,
    const std::optional<GURL>& learn_more_link) {
  return OverrideTopError(l10n_util::GetStringUTF8(error_message_id),
                          learn_more_link);
}

void DiagnosticsBuilder::AddEntry(EntryBuilder entry_builder) {
  // Note that `entry_builder.overridden_top_error_` might be moved inside this
  // block.
  if (!diagnostics_->top_error &&
      entry_builder.entry_->status == Status::kFail) {
    // Need to update the diagnostics's top error.
    if (entry_builder.overridden_top_error_) {
      diagnostics_->top_error = std::move(entry_builder.overridden_top_error_);
    } else {
      // No overridden top error. Let's use the "explanation".
      DCHECK(entry_builder.entry_->explanation);
      diagnostics_->top_error = entry_builder.entry_->explanation.Clone();
    }
  }

  diagnostics_->entries.push_back(std::move(entry_builder.entry_));
}

}  // namespace guest_os
