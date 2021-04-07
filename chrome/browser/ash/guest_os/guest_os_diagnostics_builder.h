// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_DIAGNOSTICS_BUILDER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_DIAGNOSTICS_BUILDER_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/ash/guest_os/guest_os_diagnostics.mojom.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"

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

    EntryBuilder(EntryBuilder&&);
    ~EntryBuilder();

    // Set the status to N/A.
    void SetNotApplicable();

    // Set the status to fail. The top error message is also set to explanation.
    void SetFail(const std::string& explanation);

    // Set the status to fail.
    void SetFail(
        const std::string& explanation,
        const std::string& top_error_message,
        const base::Optional<std::string>& learn_more_link = base::nullopt);

   private:
    mojom::DiagnosticEntryPtr entry;
    mojom::DiagnosticTopErrorPtr top_error;
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
