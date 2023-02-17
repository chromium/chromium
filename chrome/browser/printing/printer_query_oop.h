// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTER_QUERY_OOP_H_
#define CHROME_BROWSER_PRINTING_PRINTER_QUERY_OOP_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job_worker_oop.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"

namespace printing {

class PrinterQueryOop : public PrinterQuery {
 public:
  explicit PrinterQueryOop(content::GlobalRenderFrameHostId rfh_id);
  ~PrinterQueryOop() override;

  std::unique_ptr<PrintJobWorker> TransferContextToNewWorker(
      PrintJob* print_job) override;

 protected:
  // Local callback wrappers for Print Backend Service mojom call.  Virtual to
  // support testing.
  virtual void OnDidUseDefaultSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  virtual void OnDidAskUserForSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings);
#endif
  void OnDidUpdatePrintSettings(const std::string& device_name,
                                SettingsCallback callback,
                                mojom::PrintSettingsResultPtr print_settings);

  void UseDefaultSettings(SettingsCallback callback) override;
  void GetSettingsWithUI(uint32_t document_page_count,
                         bool has_selection,
                         bool is_scripted,
                         SettingsCallback callback) override;
  void UpdatePrintSettings(base::Value::Dict new_settings,
                           SettingsCallback callback) override;

  // Mojo support to send messages from UI thread.
  void SendUseDefaultSettings(SettingsCallback callback);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void SendAskUserForSettings(uint32_t document_page_count,
                              bool has_selection,
                              bool is_scripted,
                              SettingsCallback callback);
#endif

  // Used by `TransferContextToNewWorker()`.  Virtual to support testing.
  virtual std::unique_ptr<PrintJobWorkerOop> CreatePrintJobWorker(
      PrintJob* print_job);

  mojom::PrintTargetType print_target_type() const {
    return print_target_type_;
  }

 private:
  mojom::PrintTargetType print_target_type_ =
      mojom::PrintTargetType::kDirectToDevice;

  base::WeakPtrFactory<PrinterQueryOop> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTER_QUERY_OOP_H_
