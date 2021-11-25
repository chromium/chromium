// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OOP_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OOP_H_

#include "base/values.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom-forward.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_OOP_PRINTING)
#error "OOP printing must be enabled"
#endif

namespace printing {

class PrintJobWorkerOop : public PrintJobWorker {
 public:
  PrintJobWorkerOop(int render_process_id, int render_frame_id);
  PrintJobWorkerOop(const PrintJobWorkerOop&) = delete;
  PrintJobWorkerOop& operator=(const PrintJobWorkerOop&) = delete;
  ~PrintJobWorkerOop() override;

 protected:
  // Local callback wrapper for Print Backend Service mojom call.  Virtual to
  // support testing.
  virtual void OnDidStartPrinting(mojom::ResultCode result);

  // `PrintJobWorker` overrides.
  void UpdatePrintSettings(base::Value new_settings,
                           SettingsCallback callback) override;

 private:
  // Local callback wrapper for Print Backend Service mojom call.
  void OnDidUpdatePrintSettings(const std::string& device_name,
                                SettingsCallback callback,
                                mojom::PrintSettingsResultPtr print_settings);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OOP_H_
