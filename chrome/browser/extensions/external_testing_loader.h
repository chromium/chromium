// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_TESTING_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_TESTING_LOADER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_loader.h"

namespace extensions {

// A simplified version of ExternalPrefLoader that loads the dictionary
// from json data specified in a string.
class ExternalTestingLoader : public ExternalLoader {
 public:
  ExternalTestingLoader(const std::string& json_data,
                        const base::FilePath& fake_base_path);
  ExternalTestingLoader(const ExternalTestingLoader&) = delete;
  ExternalTestingLoader& operator=(const ExternalTestingLoader&) = delete;

  // ExternalLoader:
  const base::FilePath GetBaseCrxFilePath() override;

  void StartLoading() override;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  ~ExternalTestingLoader() override;

  base::FilePath fake_base_path_;
  base::Value::Dict testing_prefs_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_TESTING_LOADER_H_
