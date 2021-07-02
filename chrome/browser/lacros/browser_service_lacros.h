// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_BROWSER_SERVICE_LACROS_H_
#define CHROME_BROWSER_LACROS_BROWSER_SERVICE_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "mojo/public/cpp/bindings/receiver.h"

// BrowserSerivce's Lacros implementation.
// This handles the requests from ash-chrome.
class BrowserServiceLacros : public crosapi::mojom::BrowserService {
 public:
  BrowserServiceLacros();
  BrowserServiceLacros(const BrowserServiceLacros&) = delete;
  BrowserServiceLacros& operator=(const BrowserServiceLacros&) = delete;
  ~BrowserServiceLacros() override;

  // crosapi::mojom::BrowserService:
  void REMOVED_0(REMOVED_0Callback callback) override;
  void REMOVED_2(crosapi::mojom::BrowserInitParamsPtr) override;
  void NewWindow(bool incognito, NewWindowCallback callback) override;
  void NewTab(NewTabCallback callback) override;
  void RestoreTab(RestoreTabCallback callback) override;
  void GetFeedbackData(GetFeedbackDataCallback callback) override;
  void GetHistograms(GetHistogramsCallback callback) override;
  void GetActiveTabUrl(GetActiveTabUrlCallback callback) override;
  void UpdateDeviceAccountPolicy(const std::vector<uint8_t>& policy) override;

 private:
  void OnSystemInformationReady(
      GetFeedbackDataCallback callback,
      std::unique_ptr<system_logs::SystemLogsResponse> sys_info);

  void OnGetCompressedHistograms(GetHistogramsCallback callback,
                                 const std::string& compressed_histogram);

  mojo::Receiver<crosapi::mojom::BrowserService> receiver_{this};
  base::WeakPtrFactory<BrowserServiceLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_BROWSER_SERVICE_LACROS_H_
