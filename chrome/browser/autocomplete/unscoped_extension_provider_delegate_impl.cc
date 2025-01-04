// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/unscoped_extension_provider_delegate_impl.h"

#include <cstddef>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/omnibox/omnibox_input_watcher_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/unscoped_extension_provider.h"

UnscopedExtensionProviderDelegateImpl::UnscopedExtensionProviderDelegateImpl(
    Profile* profile,
    UnscopedExtensionProvider* provider)
    : profile_(profile), provider_(provider) {
  CHECK(provider_);
  omnibox_input_observation_.Observe(
      OmniboxInputWatcherFactory::GetForBrowserContext(profile_));
}

UnscopedExtensionProviderDelegateImpl::
    ~UnscopedExtensionProviderDelegateImpl() = default;

bool UnscopedExtensionProviderDelegateImpl::Start(
    const AutocompleteInput& input,
    bool minimal_changes,
    std::set<std::string> unscoped_mode_extension_ids) {
  bool want_asynchronous_matches = !input.omit_asynchronous_matches();

  if (minimal_changes) {
    // TODO(378538411): return previous cached matches if there are only minimal
    // changes.
  } else {
    // TODO(378538411): reset last input and suggest matches.
    for (const std::string& extension_id : unscoped_mode_extension_ids) {
      extensions::ExtensionOmniboxEventRouter::OnInputChanged(
          profile_, extension_id, base::UTF16ToUTF8(input.text()),
          current_request_id_);
    }
    set_done(false);
  }
  return want_asynchronous_matches;
}

void UnscopedExtensionProviderDelegateImpl::IncrementRequestId() {
  current_request_id_++;
}

// Input has been accepted, so end this input session. Ensure
// the `OnInputCancelled()` event is not sent, and no more stray
// suggestions_ready events are handled.
void UnscopedExtensionProviderDelegateImpl::OnOmniboxInputEntered() {
  // TODO(378538411): make sure this called when a match created by this class
  //   is selected.
  IncrementRequestId();
}
