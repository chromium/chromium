// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_DICTIONARY_EVENT_ROUTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_DICTIONARY_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
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

  ExtensionDictionaryEventRouter(const ExtensionDictionaryEventRouter&) =
      delete;
  ExtensionDictionaryEventRouter& operator=(
      const ExtensionDictionaryEventRouter&) = delete;

  virtual ~ExtensionDictionaryEventRouter();

  // SpellcheckCustomDictionary::Observer implementation.
  void OnCustomDictionaryLoaded() override;
  void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& dictionary_change) override;

  void DispatchLoadedEventIfLoaded();

 private:
  raw_ptr<content::BrowserContext> context_;
  base::WeakPtr<SpellcheckService> service_;
  bool loaded_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_DICTIONARY_EVENT_ROUTER_H_
