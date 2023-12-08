// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_INITIALIZATION_HOST_IMPL_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_INITIALIZATION_HOST_IMPL_H_

#include "components/spellcheck/common/spellcheck.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

// Implementation of SpellCheckHost involving Chrome-only features.
class SpellCheckInitializationHostImpl
    : public spellcheck::mojom::SpellCheckInitializationHost {
 public:
  explicit SpellCheckInitializationHostImpl(int render_process_id);

  SpellCheckInitializationHostImpl(const SpellCheckInitializationHostImpl&) =
      delete;
  SpellCheckInitializationHostImpl& operator=(
      const SpellCheckInitializationHostImpl&) = delete;

  ~SpellCheckInitializationHostImpl() override;

  static void Create(
      int render_process_id,
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckInitializationHost>
          receiver);

 private:
  // spellcheck::mojom::SpellCheckInitializationHost:
  void RequestDictionary() override;

  // The process ID of the renderer.
  const int render_process_id_;
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_INITIALIZATION_HOST_IMPL_H_
