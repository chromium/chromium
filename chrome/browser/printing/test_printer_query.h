// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_TEST_PRINTER_QUERY_H_
#define CHROME_BROWSER_PRINTING_TEST_PRINTER_QUERY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"

namespace printing {

class TestPrintQueriesQueue : public PrintQueriesQueue {
 public:
  TestPrintQueriesQueue() = default;

  // Creates a TestPrinterQuery. Sets up the printer query with the printer
  // settings indicated by |printable_offset_x_|, |printable_offset_y_|, and
  // |printer_type_|.
  std::unique_ptr<PrinterQuery> CreatePrinterQuery(
      int render_process_id,
      int render_frame_id) override;

  // Sets the printer's printable area offsets to |offset_x| and |offset_y|,
  // which should be in pixels. Used to fill in printer settings that would
  // normally be filled in by the backend |PrintingContext|.
  void SetupPrinterOffsets(int offset_x, int offset_y);

#if defined(OS_WIN)
  // Sets the printer type to |type|. Used to fill in printer settings that
  // would normally be filled in by the backend |PrintingContext|.
  void SetupPrinterType(PrintSettings::PrinterType type);
#endif

 private:
  ~TestPrintQueriesQueue() override {}

#if defined(OS_WIN)
  PrintSettings::PrinterType printer_type_;
#endif
  int printable_offset_x_;
  int printable_offset_y_;

  DISALLOW_COPY_AND_ASSIGN(TestPrintQueriesQueue);
};

class TestPrinterQuery : public PrinterQuery {
 public:
  // Can only be called on the IO thread, since this inherits from PrinterQuery.
  TestPrinterQuery(int render_process_id, int render_frame_id);
  ~TestPrinterQuery() override;

  // Updates the current settings with |new_settings| dictionary values. Also
  // fills in the settings with values from |offsets_| and |printer_type_| that
  // would normally be filled in by the PrintingContext.
  void SetSettings(base::Value new_settings,
                   base::OnceClosure callback) override;

#if defined(OS_WIN)
  // Sets |printer_type_| to |type|. Should be called before SetSettings().
  void SetPrinterType(PrintSettings::PrinterType type);
#endif

  // Sets printer offsets to |offset_x| and |offset_y|, which should be in DPI.
  // Should be called before SetSettings().
  void SetPrintableAreaOffsets(int offset_x, int offset_y);

  // Intentional no-op.
  void StopWorker() override;

 private:
  base::Optional<gfx::Point> offsets_;
#if defined(OS_WIN)
  base::Optional<PrintSettings::PrinterType> printer_type_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestPrinterQuery);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_TEST_PRINTER_QUERY_H_
