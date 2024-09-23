// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LACROS_SELECTION_LOADER_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSAPI_LACROS_SELECTION_LOADER_FACTORY_H_

#include <memory>

namespace crosapi {
class LacrosSelectionLoader;

// Abstract interface to create LacrosSelectionLoader.
class LacrosSelectionLoaderFactory {
 public:
  virtual ~LacrosSelectionLoaderFactory() = default;

  // Interface to create LacrosSelectionLoader for rootfs/stateful.
  virtual std::unique_ptr<LacrosSelectionLoader> CreateRootfsLacrosLoader() = 0;
  virtual std::unique_ptr<LacrosSelectionLoader>
  CreateStatefulLacrosLoader() = 0;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LACROS_SELECTION_LOADER_FACTORY_H_
