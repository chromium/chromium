// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_TEST_PRINT_JOB_H_
#define CHROME_BROWSER_PRINTING_TEST_PRINT_JOB_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job.h"
#include "printing/print_settings.h"

namespace printing {

class PrinterQuery;

class TestPrintJob : public PrintJob {
 public:
  // Create a empty PrintJob. When initializing with this constructor,
  // post-constructor initialization must be done with Initialize().
  TestPrintJob() = default;

  // Getters for values stored by TestPrintJob in Start...Converter functions.
  const gfx::Size& page_size() const { return page_size_; }
  const gfx::Rect& content_area() const { return content_area_; }
  const gfx::Point& physical_offsets() const { return physical_offsets_; }
#if defined(OS_WIN)
  PrintSettings::PrinterType type() const { return type_; }
#endif

  // content::NotificationObserver implementation. Deliberately empty.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {}

  // All remaining functions are PrintJob implementation.
  void Initialize(std::unique_ptr<PrinterQuery> query,
                  const base::string16& name,
                  int page_count) override;

  // Sets |job_pending_| to true.
  void StartPrinting() override;

  // Sets |job_pending_| to false and deletes the worker.
  void Stop() override;

  // Sets |job_pending_| to false and deletes the worker.
  void Cancel() override;

  // Intentional no-op, returns true.
  bool FlushJob(base::TimeDelta timeout) override;

#if defined(OS_WIN)
  // These functions fill in the corresponding member variables based on the
  // arguments passed in.
  void StartPdfToEmfConversion(scoped_refptr<base::RefCountedMemory> bytes,
                               const gfx::Size& page_size,
                               const gfx::Rect& content_area) override;

  void StartPdfToPostScriptConversion(
      scoped_refptr<base::RefCountedMemory> bytes,
      const gfx::Rect& content_area,
      const gfx::Point& physical_offsets,
      bool ps_level2) override;

  void StartPdfToTextConversion(scoped_refptr<base::RefCountedMemory> bytes,
                                const gfx::Size& page_size) override;
#endif  // defined(OS_WIN)

 private:
  ~TestPrintJob() override;

  gfx::Size page_size_;
  gfx::Rect content_area_;
  gfx::Point physical_offsets_;
#if defined(OS_WIN)
  PrintSettings::PrinterType type_;
#endif
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_TEST_PRINT_JOB_H_
