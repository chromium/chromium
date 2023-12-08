// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_initialization_host_impl.h"

#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

SpellCheckInitializationHostImpl::SpellCheckInitializationHostImpl(
    int render_process_id)
    : render_process_id_(render_process_id) {}

SpellCheckInitializationHostImpl::~SpellCheckInitializationHostImpl() = default;

// static
void SpellCheckInitializationHostImpl::Create(
    int render_process_id,
    mojo::PendingReceiver<spellcheck::mojom::SpellCheckInitializationHost>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SpellCheckInitializationHostImpl>(render_process_id),
      std::move(receiver));
}

void SpellCheckInitializationHostImpl::RequestDictionary() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* host = content::RenderProcessHost::FromID(render_process_id_);
  if (!host) {
    return;
  }
  // The renderer has requested that we initialize its spellchecker. This
  // generally should only be called once per session, as after the first
  // call, future renderers will be passed the initialization information
  // on startup (or when the dictionary changes in some way).
  SpellcheckService* spellcheck =
      SpellcheckServiceFactory::GetForContext(host->GetBrowserContext());
  if (!spellcheck) {
    return;  // Teardown.
  }

  // The spellchecker initialization already started and finished; just
  // send it to the renderer.
  if (host) {
    spellcheck->InitForRenderer(host);
  }

  // TODO(rlp): Ensure that we do not initialize the hunspell dictionary
  // more than once if we get requests from different renderers.
}
