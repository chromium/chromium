// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_DICTIONARY_LOAD_OBSERVER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_DICTIONARY_LOAD_OBSERVER_H_

#include "base/functional/callback.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

// DictionaryLoadObserver is used when blocking until the
// SpellcheckCustomDictionary finishes loading. As soon as the
// SpellcheckCustomDictionary finishes loading, the message loop is quit.
class DictionaryLoadObserver : public SpellcheckCustomDictionary::Observer {
 public:
  explicit DictionaryLoadObserver(base::OnceClosure quit_task);

  DictionaryLoadObserver(const DictionaryLoadObserver&) = delete;
  DictionaryLoadObserver& operator=(const DictionaryLoadObserver&) = delete;

  virtual ~DictionaryLoadObserver();

  // SpellcheckCustomDictionary::Observer implementation.
  void OnCustomDictionaryLoaded() override;
  void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& dictionary_change) override;

 private:
  base::OnceClosure quit_task_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_DICTIONARY_LOAD_OBSERVER_H_
