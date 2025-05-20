// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PAGE_CONTEXT_FETCHER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PAGE_CONTEXT_FETCHER_H_

#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

void FetchPageContext(
    FocusedTabData focused_tab_data,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::GetContextFromFocusedTabCallback callback);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PAGE_CONTEXT_FETCHER_H_
