// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_CHROME_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_LACROS_LACROS_CHROME_SERVICE_DELEGATE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/lacros/lacros_chrome_service_delegate.h"
#include "components/feedback/system_logs/system_logs_source.h"

// Chrome implementation of LacrosChromeServiceDelegate.
class LacrosChromeServiceDelegateImpl
    : public chromeos::LacrosChromeServiceDelegate {
 public:
  LacrosChromeServiceDelegateImpl();
  LacrosChromeServiceDelegateImpl(const LacrosChromeServiceDelegateImpl&) =
      delete;
  LacrosChromeServiceDelegateImpl& operator=(
      const LacrosChromeServiceDelegateImpl&) = delete;
  ~LacrosChromeServiceDelegateImpl() override;

  // chromeos::LacrosChromeServiceDelegate:
  void OnInitialized(
      const crosapi::mojom::BrowserInitParams& init_params) override;
  void NewWindow(bool incognito) override;
  void NewTab() override;
  void RestoreTab() override;
  std::string GetChromeVersion() override;
  void GetFeedbackData(GetFeedbackDataCallback callback) override;
  void GetHistograms(GetHistogramsCallback callback) override;
  GURL GetActiveTabUrl() override;

 private:
  void OnSystemInformationReady(
      GetFeedbackDataCallback callback,
      std::unique_ptr<system_logs::SystemLogsResponse> sys_info);

  void OnGetCompressedHistograms(
      GetHistogramsCallback callback,
      const std::string& compressed_histogram);

  base::WeakPtrFactory<LacrosChromeServiceDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_LACROS_CHROME_SERVICE_DELEGATE_IMPL_H_
