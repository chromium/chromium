// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_COMPONENT_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_COMPONENT_LOADER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

// A specialization of the ExternalLoader that loads a hard-coded list of
// external extensions, that should be considered components of chrome (but
// unlike Component extensions, these extensions are installed from the webstore
// and don't get access to component only APIs.
// Instances of this class are expected to be created and destroyed on the UI
// thread and they are expecting public method calls from the UI thread.
class ExternalComponentLoader : public ExternalLoader {
 public:
  explicit ExternalComponentLoader(Profile* profile);

  ExternalComponentLoader(const ExternalComponentLoader&) = delete;
  ExternalComponentLoader& operator=(const ExternalComponentLoader&) = delete;

 protected:
  void StartLoading() override;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;
  ~ExternalComponentLoader() override;

  void AddExternalExtension(const std::string& extension_id,
                            base::Value::Dict& prefs);

  // The profile that this loader is associated with. It listens for
  // preference changes for that profile.
  raw_ptr<Profile> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_COMPONENT_LOADER_H_
