// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_LOADER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"

namespace extensions {
class ExternalProviderImpl;

// Base class for gathering a list of external extensions. Subclasses
// implement loading from registry, JSON file, policy.
// Instances are owned by ExternalProviderImpl objects.
// Instances are created on the UI thread and expect public method calls from
// the UI thread. Some subclasses introduce new methods that are executed on the
// FILE thread.
// The sequence of loading the extension list:
// 1.) StartLoading() - checks if a loading task is already running
// 2.) Load() - implemented in subclasses
// 3.) LoadFinished()
// 4.) owner_->SetPrefs()
class ExternalLoader : public base::RefCountedThreadSafe<ExternalLoader> {
 public:
  ExternalLoader();
  ExternalLoader(const ExternalLoader&) = delete;
  ExternalLoader& operator=(const ExternalLoader&) = delete;

  // Specifies the provider that owns this object.
  void Init(ExternalProviderImpl* owner);

  // Called by the owner before it gets deleted.
  void OwnerShutdown();

  // Initiates the possibly asynchronous loading of extension list.
  // Implementations of this method should call LoadFinished with results.
  virtual void StartLoading() = 0;

  // Some external providers allow relative file paths to local CRX files.
  // Subclasses that want this behavior should override this method to
  // return the absolute path from which relative paths should be resolved.
  // By default, return an empty path, which indicates that relative paths
  // are not allowed.
  virtual const base::FilePath GetBaseCrxFilePath();

 protected:
  virtual ~ExternalLoader();

  virtual void LoadFinished(base::Value::Dict prefs);

  void OnUpdated(base::Value::Dict updated_prefs);

  // Returns true if this loader has an owner.
  // This is useful to know if calling LoadFinished/OnUpdated will propagate
  // prefs to our owner.
  bool has_owner() const { return owner_ != nullptr; }

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  raw_ptr<ExternalProviderImpl> owner_ = nullptr;  // weak
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_LOADER_H_
