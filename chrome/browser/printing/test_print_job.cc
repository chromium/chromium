// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/printing/test_print_job.h"
#include "printing/printed_document.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

void TestPrintJob::Initialize(std::unique_ptr<PrinterQuery> query,
                              const base::string16& name,
                              int page_count) {
  // Since we do not actually print in these tests, just let this get destroyed
  // when this function exits.
  std::unique_ptr<PrintJobWorker> worker = query->DetachWorker();

  scoped_refptr<PrintedDocument> new_doc =
      base::MakeRefCounted<PrintedDocument>(query->ExtractSettings(), name,
                                            query->cookie());

  new_doc->set_page_count(page_count);
  UpdatePrintedDocument(new_doc.get());
}

void TestPrintJob::StartPrinting() {
  set_job_pending(true);
}

void TestPrintJob::Stop() {
  set_job_pending(false);
}

void TestPrintJob::Cancel() {
  set_job_pending(false);
}

bool TestPrintJob::FlushJob(base::TimeDelta timeout) {
  return true;
}

#if defined(OS_WIN)
void TestPrintJob::StartPdfToEmfConversion(
    scoped_refptr<base::RefCountedMemory> bytes,
    const gfx::Size& page_size,
    const gfx::Rect& content_area) {
  page_size_ = page_size;
  content_area_ = content_area;
  type_ = PrintSettings::PrinterType::TYPE_NONE;
}

void TestPrintJob::StartPdfToPostScriptConversion(
    scoped_refptr<base::RefCountedMemory> bytes,
    const gfx::Rect& content_area,
    const gfx::Point& physical_offsets,
    bool ps_level2) {
  content_area_ = content_area;
  physical_offsets_ = physical_offsets;
  type_ = ps_level2 ? PrintSettings::PrinterType::TYPE_POSTSCRIPT_LEVEL2
                    : PrintSettings::PrinterType::TYPE_POSTSCRIPT_LEVEL3;
}

void TestPrintJob::StartPdfToTextConversion(
    scoped_refptr<base::RefCountedMemory> bytes,
    const gfx::Size& page_size) {
  page_size_ = page_size;
  type_ = PrintSettings::PrinterType::TYPE_TEXTONLY;
}
#endif  // defined(OS_WIN)

TestPrintJob::~TestPrintJob() {
  set_job_pending(false);
}

}  // namespace printing
