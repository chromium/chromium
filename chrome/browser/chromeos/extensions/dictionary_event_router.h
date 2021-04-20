// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DICTIONARY_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DICTIONARY_EVENT_ROUTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// Event router class for custom dictionary events.
class ExtensionDictionaryEventRouter
    : public SpellcheckCustomDictionary::Observer {
 public:
  explicit ExtensionDictionaryEventRouter(content::BrowserContext* context);
  virtual ~ExtensionDictionaryEventRouter();

  // SpellcheckCustomDictionary::Observer implementation.
  void OnCustomDictionaryLoaded() override;
  void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& dictionary_change) override;

  void DispatchLoadedEventIfLoaded();

 private:
  content::BrowserContext* context_;
  base::WeakPtr<SpellcheckService> service_;
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDictionaryEventRouter);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DICTIONARY_EVENT_ROUTER_H_
