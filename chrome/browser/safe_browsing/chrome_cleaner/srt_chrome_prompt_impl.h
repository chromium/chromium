// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_CHROME_PROMPT_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_CHROME_PROMPT_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_scanner_results.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace safe_browsing {

// Implementation of the ChromePrompt Mojo interface. Must be constructed and
// destructed on the IO thread.
class ChromePromptImpl : public chrome_cleaner::mojom::ChromePrompt {
 public:
  using OnPromptUser = base::OnceCallback<void(
      ChromeCleanerScannerResults&&,
      chrome_cleaner::mojom::ChromePrompt::PromptUserCallback)>;

  ChromePromptImpl(extensions::ExtensionService* extension_service,
                   chrome_cleaner::mojom::ChromePromptRequest request,
                   base::Closure on_connection_closed,
                   OnPromptUser on_prompt_user);
  ~ChromePromptImpl() override;

  void PromptUser(
      const std::vector<base::FilePath>& files_to_delete,
      const base::Optional<std::vector<base::string16>>& registry_keys,
      const base::Optional<std::vector<base::string16>>& extension_ids,
      chrome_cleaner::mojom::ChromePrompt::PromptUserCallback callback)
      override;

  void DisableExtensions(
      const std::vector<base::string16>& extension_ids,
      chrome_cleaner::mojom::ChromePrompt::DisableExtensionsCallback callback)
      override;

 private:
  mojo::Binding<chrome_cleaner::mojom::ChromePrompt> binding_;
  extensions::ExtensionService* extension_service_;
  OnPromptUser on_prompt_user_;
  base::Optional<std::vector<base::string16>> extension_ids_;

  DISALLOW_COPY_AND_ASSIGN(ChromePromptImpl);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_CHROME_PROMPT_IMPL_H_
