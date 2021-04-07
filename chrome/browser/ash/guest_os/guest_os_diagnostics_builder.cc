// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_diagnostics_builder.h"

#include <utility>

#include "base/optional.h"
#include "chrome/browser/ash/guest_os/guest_os_diagnostics.mojom.h"

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
    : entry{mojom::DiagnosticEntry::New(requirement,
                                        Status::kPass,
                                        base::nullopt)} {}
DiagnosticsBuilder::EntryBuilder::EntryBuilder(EntryBuilder&&) = default;
DiagnosticsBuilder::EntryBuilder::~EntryBuilder() = default;

void DiagnosticsBuilder::EntryBuilder::SetNotApplicable() {
  entry->status = Status::kNotApplicable;
}

void DiagnosticsBuilder::EntryBuilder::SetFail(const std::string& explanation) {
  SetFail(explanation, /*top_error_message=*/explanation);
}

void DiagnosticsBuilder::EntryBuilder::SetFail(
    const std::string& explanation,
    const std::string& top_error_message,
    const base::Optional<std::string>& learn_more_link) {
  entry->status = Status::kFail;
  entry->explanation = explanation;

  top_error =
      mojom::DiagnosticTopError::New(top_error_message, learn_more_link);
}

void DiagnosticsBuilder::AddEntry(EntryBuilder entry_builder) {
  diagnostics_->entries.push_back(std::move(entry_builder.entry));
  if (diagnostics_->top_error.is_null()) {
    diagnostics_->top_error = std::move(entry_builder.top_error);
  }
}

}  // namespace guest_os
