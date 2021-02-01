// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_COMPONENT_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_COMPONENT_LOADER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/profiles/profile.h"

namespace base {
class DictionaryValue;
}

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

 protected:
  void StartLoading() override;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;
  ~ExternalComponentLoader() override;

  void AddExternalExtension(const std::string& extension_id,
                            base::DictionaryValue* prefs);

  // The profile that this loader is associated with. It listens for
  // preference changes for that profile.
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(ExternalComponentLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_COMPONENT_LOADER_H_
