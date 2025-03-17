// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_preview_webcontents_manager.h"

#include "base/check_deref.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/printing/print_preview/print_preview_webcontents_adapter_ash.h"
#include "chrome/browser/chromeos/printing/print_preview/print_settings_converter.h"
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"

using ::printing::mojom::RequestPrintPreviewParamsPtr;

namespace chromeos {

namespace {

PrintPreviewWebcontentsManager* g_instance_for_testing = nullptr;

crosapi::mojom::PrintPreviewCrosDelegate* g_delegate_instance_for_testing =
    nullptr;

crosapi::mojom::PrintPreviewCrosDelegate& print_preview_cros_delegate() {
  if (g_delegate_instance_for_testing) {
    return CHECK_DEREF(g_delegate_instance_for_testing);
  }
  return CHECK_DEREF(crosapi::CrosapiManager::Get()
                         ->crosapi_ash()
                         ->print_preview_webcontents_adapter_ash());
}

}  // namespace

// static
PrintPreviewWebcontentsManager* PrintPreviewWebcontentsManager::Get() {
  if (g_instance_for_testing) {
    return g_instance_for_testing;
  }

  static base::NoDestructor<PrintPreviewWebcontentsManager> instance;
  return instance.get();
}

// static
void PrintPreviewWebcontentsManager::SetInstanceForTesting(
    PrintPreviewWebcontentsManager* manager) {
  g_instance_for_testing = manager;
}

// static
void PrintPreviewWebcontentsManager::ResetInstanceForTesting() {
  g_instance_for_testing = nullptr;
}

PrintPreviewWebcontentsManager::PrintPreviewWebcontentsManager() = default;
PrintPreviewWebcontentsManager::~PrintPreviewWebcontentsManager() = default;

void PrintPreviewWebcontentsManager::Initialize() {
  // Register the C++ (non-mojo) client.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->print_preview_webcontents_adapter_ash()
        ->RegisterAshClient(this);
  }
}

void PrintPreviewWebcontentsManager::GeneratePrintPreview(
    const base::UnguessableToken& token,
    crosapi::mojom::PrintSettingsPtr settings,
    GeneratePrintPreviewCallback callback) {
  const auto found_content_iter = token_to_webcontents_.find(token);
  if (found_content_iter == token_to_webcontents_.end()) {
    mojo::ReportBadMessage(
        "Bad token, can only be called by a valid print preview instance.");
    return;
  }

  PrintViewManagerCros* view_manager =
      PrintViewManagerCros::FromWebContents(found_content_iter->second);
  if (!view_manager) {
    PRINTER_LOG(ERROR) << "Failed to start generating a print preview.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  view_manager->HandleGeneratePrintPreview(
      SerializePrintSettings(std::move(settings)));
  std::move(callback).Run(/*success=*/true);
}

void PrintPreviewWebcontentsManager::HandleDialogClosed(
    const base::UnguessableToken& token,
    HandleDialogClosedCallback callback) {
  content::WebContents* webcontents = RemoveTokenMapping(token);
  if (!webcontents) {
    // Entry already removed, no-opt. Handles potential race condition if
    // initiator crashes in the midst of closing the print dialog.
    std::move(callback).Run(/*success=*/false);
    return;
  }

  PrintViewManagerCros::FromWebContents(webcontents)
      ->HandlePrintPreviewRemoved();

  // TODO(jimmyxgong): Address other potential failure cases when implemented.
  std::move(callback).Run(/*success=*/true);
}

void PrintPreviewWebcontentsManager::RequestPrintPreview(
    const base::UnguessableToken& token,
    content::WebContents* webcontents,
    ::printing::mojom::RequestPrintPreviewParamsPtr params) {
  CHECK(webcontents);
  token_to_webcontents_[token] = webcontents;

  print_preview_cros_delegate().RequestPrintPreview(
      token, std::move(params),
      base::BindOnce(
          &PrintPreviewWebcontentsManager::OnRequestPrintPreviewCallback,
          weak_ptr_factory_.GetWeakPtr()));
}

void PrintPreviewWebcontentsManager::PrintPreviewDone(
    const base::UnguessableToken& token) {
  if (!RemoveTokenMapping(token)) {
    // Entry already removed, no-opt. Handles potential race condition if
    // initiator crashes in the midst of closing the print dialog.
    return;
  }

  print_preview_cros_delegate().PrintPreviewDone(
      token, base::BindOnce(
                 &PrintPreviewWebcontentsManager::OnPrintPreviewDoneCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PrintPreviewWebcontentsManager::OnRequestPrintPreviewCallback(
    bool success) {
  if (!success) {
    PRINTER_LOG(ERROR)
        << "PrintPreviewCros failed to request print preview dialog.";
  }
}

void PrintPreviewWebcontentsManager::OnPrintPreviewDoneCallback(bool success) {
  if (!success) {
    PRINTER_LOG(ERROR) << "PrintPreviewCros failed to close print dialog";
  }
}

content::WebContents* PrintPreviewWebcontentsManager::RemoveTokenMapping(
    const base::UnguessableToken& token) {
  // Confirm mappings exist.
  const auto found_content = token_to_webcontents_.find(token);
  if (found_content == token_to_webcontents_.end()) {
    return nullptr;
  }

  // Remove webcontents mappings.
  content::WebContents* webcontents = found_content->second;
  token_to_webcontents_.erase(found_content->first);
  return webcontents;
}

void PrintPreviewWebcontentsManager::SetPrintPreviewCrosDelegateForTesting(
    crosapi::mojom::PrintPreviewCrosDelegate* delegate) {
  g_delegate_instance_for_testing = delegate;
}

}  // namespace chromeos
