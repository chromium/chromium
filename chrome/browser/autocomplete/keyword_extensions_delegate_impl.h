// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// KeywordExtensionsDelegateImpl contains the extensions-only logic used by
// KeywordProvider. Overrides KeywordExtensionsDelegate which does nothing.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_EXTENSIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_EXTENSIONS_DELEGATE_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/keyword_extensions_delegate.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Should not be included when extensions are disabled"
#endif

class Profile;

class KeywordExtensionsDelegateImpl : public KeywordExtensionsDelegate,
                                      public content::NotificationObserver {
 public:
  KeywordExtensionsDelegateImpl(Profile* profile, KeywordProvider* provider);
  ~KeywordExtensionsDelegateImpl() override;

  // KeywordExtensionsDelegate:
  void DeleteSuggestion(const TemplateURL* template_url,
                        const base::string16& suggestion_text) override;

 private:
  // KeywordExtensionsDelegate:
  void IncrementInputId() override;
  bool IsEnabledExtension(const std::string& extension_id) override;
  bool Start(const AutocompleteInput& input,
             bool minimal_changes,
             const TemplateURL* template_url,
             const base::string16& remaining_input) override;
  void EnterExtensionKeywordMode(const std::string& extension_id) override;
  void MaybeEndExtensionKeywordMode() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  ACMatches* matches() { return &provider_->matches_; }
  void set_done(bool done) {
    provider_->done_ = done;
  }

  // Notifies the KeywordProvider about asynchronous updates from the extension.
  void OnProviderUpdate(bool updated_matches);

  // Identifies the current input state. This is incremented each time the
  // autocomplete edit's input changes in any way. It is used to tell whether
  // suggest results from the extension are current.
  int current_input_id_;

  // The input state at the time we last asked the extension for suggest
  // results.
  AutocompleteInput extension_suggest_last_input_;

  // We remember the last suggestions we've received from the extension in case
  // we need to reset our matches without asking the extension again.
  std::vector<AutocompleteMatch> extension_suggest_matches_;

  // If non-empty, holds the ID of the extension whose keyword is currently in
  // the URL bar while the autocomplete popup is open.
  std::string current_keyword_extension_id_;

  Profile* profile_;

  // The owner of this class.
  KeywordProvider* provider_;

  content::NotificationRegistrar registrar_;

  // We need our input IDs to be unique across all profiles, so we keep a global
  // UID that each provider uses.
  static int global_input_uid_;

  DISALLOW_COPY_AND_ASSIGN(KeywordExtensionsDelegateImpl);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_EXTENSIONS_DELEGATE_IMPL_H_
