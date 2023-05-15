// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smart_reader/smart_reader_client_impl.h"

#include <utility>

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace smart_reader {

SmartReaderClientImpl::SmartReaderClientImpl() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Bind receiver and pass remote to SmartReaderManagerAsh.
  if (chromeos::LacrosService::Get()
          ->IsSupported<crosapi::mojom::SmartReaderClient>()) {
    chromeos::LacrosService::Get()
        ->BindPendingReceiverOrRemote<
            mojo::PendingRemote<crosapi::mojom::SmartReaderClient>,
            &crosapi::mojom::Crosapi::BindSmartReaderClient>(
            receiver_.BindNewPipeAndPassRemote());
  }
#endif
}

SmartReaderClientImpl::~SmartReaderClientImpl() = default;

void SmartReaderClientImpl::CollectCurrentPageContent() {}

void SmartReaderClientImpl::GetPageContent(GetPageContentCallback callback) {
  crosapi::mojom::SmartReaderPageContentPtr smart_reader_content_ptr =
      crosapi::mojom::SmartReaderPageContent::New(title_, url_, contents_);
  std::move(callback).Run(std::move(smart_reader_content_ptr));
}
}  // namespace smart_reader
