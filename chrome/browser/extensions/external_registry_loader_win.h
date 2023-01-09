// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_LOADER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_LOADER_WIN_H_

#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/browser/extensions/external_loader.h"

namespace extensions {

class ExternalRegistryLoader : public ExternalLoader {
 public:
  ExternalRegistryLoader();

  ExternalRegistryLoader(const ExternalRegistryLoader&) = delete;
  ExternalRegistryLoader& operator=(const ExternalRegistryLoader&) = delete;

 protected:
  ~ExternalRegistryLoader() override;  // protected for unit test.

  void StartLoading() override;

  // Overridden to mock registry reading in unit tests.
  virtual base::Value::Dict LoadPrefsOnBlockingThread();

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  void LoadOnBlockingThread();
  void CompleteLoadAndStartWatchingRegistry(base::Value::Dict prefs);
  void UpatePrefsOnBlockingThread();
  void OnRegistryKeyChanged(base::win::RegKey* key);

  scoped_refptr<base::SequencedTaskRunner> GetOrCreateTaskRunner();

  // Whether or not we attempted to observe registry.
  bool attempted_watching_registry_;

  base::win::RegKey hklm_key_;
  base::win::RegKey hkcu_key_;

  // Task runner where registry keys are read.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_LOADER_WIN_H_
