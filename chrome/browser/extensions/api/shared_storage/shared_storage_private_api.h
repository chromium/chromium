// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SHARED_STORAGE_SHARED_STORAGE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SHARED_STORAGE_SHARED_STORAGE_PRIVATE_API_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/extension_function.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include <optional>
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PrefRegistrySimple;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {
#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace shared_storage {
void RegisterProfilePrefs(PrefRegistrySimple* registry);
}  // namespace shared_storage
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Used by gnubbyd in ash and lacros.  Stores small amounts of data in ash prefs
// which is shared by ash and lacros versions of the extension.
// TODO(b/231890240): Once Terminal SWA runs in lacros rather than ash, we can
// migrate gnubbyd back to using chrome.storage.local and remove this private
// API.
class SharedStoragePrivateGetFunction : public ExtensionFunction {
 public:
  SharedStoragePrivateGetFunction();
  SharedStoragePrivateGetFunction(const SharedStoragePrivateGetFunction&) =
      delete;
  SharedStoragePrivateGetFunction& operator=(
      const SharedStoragePrivateGetFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("sharedStoragePrivate.get",
                             SHAREDSTORAGEPRIVATE_GET)

 protected:
  ~SharedStoragePrivateGetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnGet(std::optional<base::Value> storage);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

class SharedStoragePrivateSetFunction : public ExtensionFunction {
 public:
  SharedStoragePrivateSetFunction();
  SharedStoragePrivateSetFunction(const SharedStoragePrivateSetFunction&) =
      delete;
  SharedStoragePrivateSetFunction& operator=(
      const SharedStoragePrivateSetFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("sharedStoragePrivate.set",
                             SHAREDSTORAGEPRIVATE_SET)

 protected:
  ~SharedStoragePrivateSetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnGet(base::Value::Dict to_add, std::optional<base::Value> items);
  void OnSet();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

class SharedStoragePrivateRemoveFunction : public ExtensionFunction {
 public:
  SharedStoragePrivateRemoveFunction();
  SharedStoragePrivateRemoveFunction(
      const SharedStoragePrivateRemoveFunction&) = delete;
  SharedStoragePrivateRemoveFunction& operator=(
      const SharedStoragePrivateRemoveFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("sharedStoragePrivate.remove",
                             SHAREDSTORAGEPRIVATE_REMOVE)

 protected:
  ~SharedStoragePrivateRemoveFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnGet(std::vector<std::string> keys, std::optional<base::Value> storage);
  void OnSet();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SHARED_STORAGE_SHARED_STORAGE_PRIVATE_API_H_
