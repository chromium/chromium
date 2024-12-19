// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_input_watcher.h"
#include "components/omnibox/browser/unscoped_extension_provider.h"
#include "components/omnibox/browser/unscoped_extension_provider_delegate.h"
#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Should not be included when extensions are disabled"
#endif

class UnscopedExtensionProvider;
class Profile;

// Delegate observes extension events as well as input changes in the omnibox.
// This class is considered the intermediary between the provider (in
// components/) and the Omnibox API (in chrome/browser).
class UnscopedExtensionProviderDelegateImpl
    : public UnscopedExtensionProviderDelegate,
      public OmniboxInputWatcher::Observer,
      public OmniboxSuggestionsWatcher::Observer {
 public:
  UnscopedExtensionProviderDelegateImpl(Profile* profile,
                                        UnscopedExtensionProvider* provider);
  UnscopedExtensionProviderDelegateImpl(
      const UnscopedExtensionProviderDelegateImpl&) = delete;
  UnscopedExtensionProviderDelegateImpl& operator=(
      const UnscopedExtensionProviderDelegateImpl&) = delete;
  ~UnscopedExtensionProviderDelegateImpl() override;

  // UnscopedExtensionProviderDelegate:
  bool Start(const AutocompleteInput& input,
             bool minimal_changes,
             std::set<std::string> unscoped_mode_extension_ids) override;
  void IncrementRequestId() override;

  // OmniboxInputWatcher::Observer:
  void OnOmniboxInputEntered() override;

 private:
  void set_done(bool done) { provider_->set_done(done); }
  bool done() const { return provider_->done(); }

  // Identifies the current input state. This is incremented each time the
  // autocomplete edit's input changes in any way. It is used to tell whether
  // suggest results from the extension are current.
  int current_request_id_ = 0;

  // The input from the last request to the extension.
  AutocompleteInput extension_suggest_last_input_;

  // TODO(378538411): populate this once the suggestions logic is implemented.
  //  Saved suggestions that were received from the extension used
  //  for resetting matches without asking the extension again.
  std::vector<AutocompleteMatch> extension_suggest_matches_;

  raw_ptr<Profile> profile_;

  // The owner of this class.
  raw_ptr<UnscopedExtensionProvider> provider_;

  base::ScopedObservation<OmniboxInputWatcher, OmniboxInputWatcher::Observer>
      omnibox_input_observation_{this};
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_IMPL_H_
