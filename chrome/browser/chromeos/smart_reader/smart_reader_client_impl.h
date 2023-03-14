// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMART_READER_SMART_READER_CLIENT_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_SMART_READER_SMART_READER_CLIENT_IMPL_H_

#include <string>
#include <utility>

#include "chromeos/crosapi/mojom/smart_reader.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "mojo/public/cpp/bindings/receiver.h"
#endif

namespace smart_reader {

// SmartReaderClientImpl is the client class for CrOS Smart Reader. It is
// shared code between lacros-chrome and ash-chrome.
class SmartReaderClientImpl : public crosapi::mojom::SmartReaderClient {
 public:
  SmartReaderClientImpl();

  SmartReaderClientImpl(const SmartReaderClientImpl&) = delete;
  SmartReaderClientImpl& operator=(const SmartReaderClientImpl&) = delete;

  ~SmartReaderClientImpl() override;

  // crosapi::mojom::SmartReaderClient overrides
  void GetPageContent(GetPageContentCallback callback) override;

 protected:
  std::u16string contents_;
  std::u16string title_;
  GURL url_;

 private:
  // Will obtain the content of the current active web page and save the details
  // in this object.
  void CollectCurrentPageContent();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  mojo::Receiver<crosapi::mojom::SmartReaderClient> receiver_{this};
#endif
};
}  // namespace smart_reader

#endif  // CHROME_BROWSER_CHROMEOS_SMART_READER_SMART_READER_CLIENT_IMPL_H_
