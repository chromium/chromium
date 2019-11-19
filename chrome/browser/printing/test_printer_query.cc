// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/test_printer_query.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"
#include "printing/units.h"

namespace printing {

std::unique_ptr<PrinterQuery> TestPrintQueriesQueue::CreatePrinterQuery(
    int render_process_id,
    int render_frame_id) {
  auto test_query =
      std::make_unique<TestPrinterQuery>(render_process_id, render_frame_id);
#if defined(OS_WIN)
  test_query->SetPrinterType(printer_type_);
#endif
  test_query->SetPrintableAreaOffsets(printable_offset_x_, printable_offset_y_);

  return test_query;
}

void TestPrintQueriesQueue::SetupPrinterOffsets(int offset_x, int offset_y) {
  printable_offset_x_ = offset_x;
  printable_offset_y_ = offset_y;
}

#if defined(OS_WIN)
void TestPrintQueriesQueue::SetupPrinterType(PrintSettings::PrinterType type) {
  printer_type_ = type;
}
#endif

TestPrinterQuery::TestPrinterQuery(int render_process_id, int render_frame_id)
    : PrinterQuery(render_process_id, render_frame_id) {}

TestPrinterQuery::~TestPrinterQuery() {}

void TestPrinterQuery::SetSettings(base::Value new_settings,
                                   base::OnceClosure callback) {
  DCHECK(offsets_);
#if defined(OS_WIN)
  DCHECK(printer_type_);
#endif
  auto settings = std::make_unique<PrintSettings>();
  PrintingContext::Result result =
      PrintSettingsFromJobSettings(new_settings, settings.get())
          ? PrintingContext::OK
          : PrintingContext::FAILED;

  float device_microns_per_device_unit =
      static_cast<float>(kMicronsPerInch) / settings->device_units_per_inch();
  gfx::Size paper_size =
      gfx::Size(settings->requested_media().size_microns.width() /
                    device_microns_per_device_unit,
                settings->requested_media().size_microns.height() /
                    device_microns_per_device_unit);
  gfx::Rect paper_rect(0, 0, paper_size.width(), paper_size.height());
  paper_rect.Inset(offsets_->x(), offsets_->y());
  settings->SetPrinterPrintableArea(paper_size, paper_rect, true);
#if defined(OS_WIN)
  settings->set_printer_type(*printer_type_);
#endif

  GetSettingsDone(std::move(callback), std::move(settings), result);
}

#if defined(OS_WIN)
void TestPrinterQuery::SetPrinterType(PrintSettings::PrinterType type) {
  printer_type_ = type;
}
#endif

void TestPrinterQuery::SetPrintableAreaOffsets(int offset_x, int offset_y) {
  offsets_ = gfx::Point(offset_x, offset_y);
}

void TestPrinterQuery::StopWorker() {}

}  // namespace printing
