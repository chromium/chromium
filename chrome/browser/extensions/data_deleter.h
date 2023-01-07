// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DATA_DELETER_H_
#define CHROME_BROWSER_EXTENSIONS_DATA_DELETER_H_

#include "base/functional/callback_forward.h"

class Profile;

namespace extensions {

class Extension;

class DataDeleter {
 public:
  DataDeleter(const DataDeleter&) = delete;
  DataDeleter& operator=(const DataDeleter&) = delete;

  // Starts removing data. The extension should not be running when this is
  // called. Cookies are deleted on the current thread, local storage and
  // databases/settings are deleted asynchronously on the webkit and file
  // threads, respectively. This function must be called from the UI thread.
  // This method starts the deletion process and triggers |done_callback| when
  // the process has finished.
  static void StartDeleting(Profile* profile,
                            const Extension* extension,
                            base::OnceClosure done_callback);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DATA_DELETER_H_
