// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTER_QUERY_OOP_H_
#define CHROME_BROWSER_PRINTING_PRINTER_QUERY_OOP_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
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

  // PrinterQuery overrides:
  std::unique_ptr<PrintJobWorker> TransferContextToNewWorker(
      PrintJob* print_job) override;
#if BUILDFLAG(IS_WIN)
  void UpdatePrintableArea(PrintSettings* print_settings,
                           OnDidUpdatePrintableAreaCallback callback) override;
#endif
  void SetClientId(PrintBackendServiceManager::ClientId client_id) override;

 protected:
  // Local callback wrappers for Print Backend Service mojom call.  Some
  // wrappers are virtual to support testing.
  virtual void OnDidUseDefaultSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  virtual void OnDidAskUserForSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings);
#else
  virtual void OnDidAskUserForSettings(
      SettingsCallback callback,
      std::unique_ptr<PrintSettings> new_settings,
      mojom::ResultCode result);
#endif
  virtual void OnDidUpdatePrintSettings(
      const std::string& device_name,
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings);
#if BUILDFLAG(IS_WIN)
  void OnDidGetPaperPrintableArea(PrintSettings* print_settings,
                                  OnDidUpdatePrintableAreaCallback callback,
                                  const gfx::Rect& printable_area_um);
#endif

  void UseDefaultSettings(SettingsCallback callback) override;
  void GetSettingsWithUI(uint32_t document_page_count,
                         bool has_selection,
                         bool is_scripted,
                         SettingsCallback callback) override;
  void UpdatePrintSettings(base::Value::Dict new_settings,
                           SettingsCallback callback) override;

  // Mojo support to send messages from UI thread.
  void SendEstablishPrintingContext(
      PrintBackendServiceManager::ClientId client_id,
      const std::string& printer_name);
  void SendUpdatePrintSettings(const std::string& printer_name,
                               base::Value::Dict new_settings,
                               SettingsCallback callback);
  void SendUseDefaultSettings(SettingsCallback callback);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void SendAskUserForSettings(uint32_t document_page_count,
                              bool has_selection,
                              bool is_scripted,
                              SettingsCallback callback);
#endif

  // Used by `TransferContextToNewWorker()`.  Virtual to support testing.
  virtual std::unique_ptr<PrintJobWorkerOop> CreatePrintJobWorkerOop(
      PrintJob* print_job);

  const std::optional<PrintBackendServiceManager::ClientId>&
  print_document_client_id() const {
    return print_document_client_id_;
  }

  bool print_from_system_dialog() const { return print_from_system_dialog_; }

  const std::optional<PrintBackendServiceManager::ContextId>& context_id()
      const {
    return context_id_;
  }

 private:
  bool print_from_system_dialog_ = false;
  std::optional<PrintBackendServiceManager::ClientId> query_with_ui_client_id_;
  std::optional<PrintBackendServiceManager::ClientId> print_document_client_id_;
  std::optional<PrintBackendServiceManager::ContextId> context_id_;

  base::WeakPtrFactory<PrinterQueryOop> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTER_QUERY_OOP_H_
