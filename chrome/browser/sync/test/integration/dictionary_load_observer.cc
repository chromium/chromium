// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/dictionary_load_observer.h"

DictionaryLoadObserver::DictionaryLoadObserver(base::OnceClosure quit_task)
    : quit_task_(std::move(quit_task)) {}

DictionaryLoadObserver::~DictionaryLoadObserver() = default;

void DictionaryLoadObserver::OnCustomDictionaryLoaded() {
  std::move(quit_task_).Run();
}

void DictionaryLoadObserver::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& dictionary_change) {}
