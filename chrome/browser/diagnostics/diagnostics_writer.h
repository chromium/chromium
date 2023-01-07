// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_WRITER_H_
#define CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_WRITER_H_

#include <memory>

#include "chrome/browser/diagnostics/diagnostics_model.h"

namespace diagnostics {

// Console base class used internally.
class SimpleConsole;

class DiagnosticsWriter : public DiagnosticsModel::Observer {
 public:
  // The type of formatting done by this writer.
  enum FormatType {
    MACHINE,
    LOG,
    HUMAN
  };

  explicit DiagnosticsWriter(FormatType format);

  DiagnosticsWriter(const DiagnosticsWriter&) = delete;
  DiagnosticsWriter& operator=(const DiagnosticsWriter&) = delete;

  ~DiagnosticsWriter() override;

  // How many tests reported failure.
  int failures() { return failures_; }

  // What format are we writing things in.
  FormatType format() const { return format_; }

  // Write an informational line of text in white over black. String must be
  // UTF8 encoded. A newline will be added for non-LOG output formats.
  bool WriteInfoLine(const std::string& info_text);

  // DiagnosticsModel::Observer overrides
  void OnTestFinished(int index, DiagnosticsModel* model) override;
  void OnAllTestsDone(DiagnosticsModel* model) override;
  void OnRecoveryFinished(int index, DiagnosticsModel* model) override;
  void OnAllRecoveryDone(DiagnosticsModel* model) override;

 private:
  // Write a result block. For humans, it consists of two lines. The first line
  // has [PASS] or [FAIL] with |name| and the second line has the text in
  // |extra|. For machine and log formats, we just have [PASS] or [FAIL],
  // followed by the exact error code and the id. Name and extra strings must be
  // UTF8 encoded, as they are user-facing strings.
  bool WriteResult(bool success,
                   const std::string& id,
                   const std::string& name,
                   int outcome_code,
                   const std::string& extra);

  std::unique_ptr<SimpleConsole> console_;

  // Keeps track of how many tests reported failure.
  int failures_;
  FormatType format_;
};

}  // namespace diagnostics

#endif  // CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_WRITER_H_
