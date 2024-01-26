// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_DIAGNOSTICS_BUILDER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_DIAGNOSTICS_BUILDER_H_

#include <optional>
#include <string>

#include "chrome/browser/ash/guest_os/guest_os_diagnostics.mojom.h"
#include "url/gurl.h"

namespace guest_os {

// A helper to make building a `mojom::Diagnostics` easier.
class DiagnosticsBuilder {
 public:
  DiagnosticsBuilder();
  ~DiagnosticsBuilder();

  struct EntryBuilder {
    friend DiagnosticsBuilder;

    // The default status of the entry is pass.
    explicit EntryBuilder(const std::string& requirement);
    explicit EntryBuilder(int requirement_message_id);

    EntryBuilder(EntryBuilder&&);
    ~EntryBuilder();

    // Set the status to N/A.
    void SetNotApplicable();

    // Set the status to fail. By default, the associated top error is assumed
    // to be the same as the explanation. You can call `OverrideTopError()` to
    // change it. Also see `DiagnosticsBuilder::AddEntry()` for how the top
    // error is applied.
    EntryBuilder& SetFail(
        const std::string& explanation,
        const std::optional<GURL>& learn_more_link = std::nullopt);
    void OverrideTopError(
        const std::string& error,
        const std::optional<GURL>& learn_more_link = std::nullopt);

    // Version that accepting a message id.
    EntryBuilder& SetFail(
        int explanation_message_id,
        const std::optional<GURL>& learn_more_link = std::nullopt);
    void OverrideTopError(
        int error_message_id,
        const std::optional<GURL>& learn_more_link = std::nullopt);

   private:
    mojom::DiagnosticEntryPtr entry_;
    mojom::DiagnosticMessagePtr overridden_top_error_;
  };

  // Add a new entry. If the top error hasn't been set, the top error inside the
  // entry builder will only be used.
  void AddEntry(EntryBuilder builder);

  // Return the built diagnostics. This builder should not be used any more.
  mojom::DiagnosticsPtr Build();

 private:
  mojom::DiagnosticsPtr diagnostics_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_DIAGNOSTICS_BUILDER_H_
