// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_AUTH_ARC_FETCHER_BASE_H_
#define CHROME_BROWSER_ASH_ARC_AUTH_ARC_FETCHER_BASE_H_

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace arc {

// Base class for Arc*Fetcher classes, only used to manage the lifetime of their
// instances.
class ArcFetcherBase {
 public:
  ArcFetcherBase();
  virtual ~ArcFetcherBase();

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

 protected:
  scoped_refptr<network::SharedURLLoaderFactory>
  url_loader_factory_for_testing() {
    return url_loader_factory_for_testing_;
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_AUTH_ARC_FETCHER_BASE_H_
