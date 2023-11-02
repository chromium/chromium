// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/auth/arc_fetcher_base.h"

namespace arc {

ArcFetcherBase::ArcFetcherBase() = default;

ArcFetcherBase::~ArcFetcherBase() = default;

void ArcFetcherBase::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  url_loader_factory_for_testing_ = factory;
}

}  // namespace arc
