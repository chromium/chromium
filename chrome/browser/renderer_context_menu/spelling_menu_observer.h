// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_SPELLING_MENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_SPELLING_MENU_OBSERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/prefs/pref_member.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/spellcheck/browser/spelling_service_client.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/spellcheck_buildflags.h"

class RenderViewContextMenuProxy;
struct SpellCheckResult;

// An observer that listens to events from the RenderViewContextMenu class and
// shows suggestions from the Spelling ("do you mean") service to a context menu
// while we show it. This class implements two interfaces:
// * RenderViewContextMenuObserver
//   This interface is used for adding a menu item and update it while showing.
// * net::URLFetcherDelegate
//   This interface is used for sending a JSON_RPC request to the Spelling
//   service and retrieving its response.
// These interfaces allow this class to make a JSON-RPC call to the Spelling
// service in the background and update the context menu while showing. The
// following snippet describes how to add this class to the observer list of the
// RenderViewContextMenu class.
//
//   void RenderViewContextMenu::InitMenu() {
//     spelling_menu_observer_.reset(new SpellingMenuObserver(this));
//     if (spelling_menu_observer_.get())
//       observers_.AddObserver(spelling_menu_observer.get());
//   }
//
class SpellingMenuObserver : public RenderViewContextMenuObserver {
 public:
  explicit SpellingMenuObserver(RenderViewContextMenuProxy* proxy);

  SpellingMenuObserver(const SpellingMenuObserver&) = delete;
  SpellingMenuObserver& operator=(const SpellingMenuObserver&) = delete;

  ~SpellingMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void OnContextMenuShown(const content::ContextMenuParams& params,
                          const gfx::Rect& bounds_in_screen) override;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // A callback function called when the platform spellchecker finishes getting
  // suggestions for a misspelled word.
  void OnGetPlatformSuggestionsComplete(
      const spellcheck::PerLanguageSuggestions&
          platform_per_language_suggestions);

  // A callback for the BarrierClosure, fired when the platform (and possibly
  // remote) retrieval of suggestions completes.
  void OnGetSuggestionsComplete();

  // Registers a callback for testing when local (and possibly remote) retrieval
  // of suggestions has completed. This allows tests to wait in a run loop
  // before confirming the presence of expected menu items.
  void RegisterSuggestionsCompleteCallbackForTesting(
      base::OnceClosure callback);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  // A callback function called when the Spelling service finishes checking a
  // misspelled word.
  void OnGetRemoteSuggestionsComplete(
      SpellingServiceClient::ServiceType type,
      bool success,
      const std::u16string& text,
      const std::vector<SpellCheckResult>& results);

 private:
  // Method that starts the asynchronous retrieval of suggestions from the
  // remote enhanced spellcheck service.
  void GetRemoteSuggestions();

  // Updates the presented suggestion from the Spelling service.
  void UpdateRemoteSuggestion(SpellingServiceClient::ServiceType type,
                              bool success,
                              const std::vector<SpellCheckResult>& results);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Fires a callback for testing when local (and possibly remote) retrieval of
  // suggestions has completed. This allows tests to wait in a run loop before
  // confirming the presence of expected menu items.
  void FireSuggestionsCompleteCallbackIfNeededForTesting();
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  // The callback function for base::RepeatingTimer. This function updates the
  // "loading..." animation in the context-menu item for the given command_id.
  void OnAnimationTimerExpired(int command_id);

  // The interface to add a context-menu item and update it. This class uses
  // this interface to avoid accesing context-menu items directly.
  raw_ptr<RenderViewContextMenuProxy> proxy_;

  // Suggested words from the local spellchecker. If the spelling service
  // returns a word in this list, we hide the context-menu item to prevent
  // showing the same word twice.
  std::vector<std::u16string> suggestions_;

  // The string used for animation until we receive a response from the Spelling
  // service. The current animation just adds periods at the end of this string:
  //   'Loading' -> 'Loading.' -> 'Loading..' -> 'Loading...' (-> 'Loading')
  std::u16string loading_message_;
  size_t loading_frame_;

  // A flag represending whether a JSON-RPC call to the Spelling service
  // finished successfully and its response had a suggestion not included in the
  // ones provided by the local spellchecker. When this flag is true, we enable
  // the context-menu item so users can choose it.
  bool succeeded_;

  // The misspelled word. When we choose the "Add to dictionary" item, we add
  // this word to the custom-word dictionary.
  std::u16string misspelled_word_;

  // The string representing the result of this call. This string is a
  // suggestion when this call finished successfully. Otherwise it is error
  // text. Until we receive a response from the Spelling service, this string
  // stores the input string. (Since the Spelling service sends only misspelled
  // words, we replace these misspelled words in the input text with the
  // suggested words to create suggestion text.
  std::u16string result_;

  // The URLFetcher object used for sending a JSON-RPC request.
  std::unique_ptr<SpellingServiceClient> client_;

  // A timer used for loading animation.
  base::RepeatingTimer animation_timer_;

  // Flag indicating whether online spelling correction service is enabled. When
  // this variable is true and we right-click a misspelled word, we send a
  // JSON-RPC request to the service and retrieve suggestions.
  BooleanPrefMember integrate_spelling_service_;

  // Flag indicating whether automatic spelling correction is enabled.
  BooleanPrefMember autocorrect_spelling_;

  // Flag indicating that suggestions from the remote spelling service may be
  // available.
  bool use_remote_suggestions_ = false;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Flag indicating that suggestions should be retrieved asynchronously from
  // the platform spellchecker (effectively just Windows versions > Win7).
  bool use_platform_suggestions_ = false;

  // Barrier closure for completion of both remote and local check.
  base::RepeatingClosure completion_barrier_;

  // Used for caching remote results in case async platform suggestion retrieval
  // has not completed.
  SpellingServiceClient::ServiceType remote_service_type_ =
      SpellingServiceClient::SUGGEST;
  std::vector<SpellCheckResult> remote_results_;

  // Callback registered using RegisterSuggestionsCompleteCallbackForTesting.
  base::OnceClosure suggestions_complete_callback_for_testing_;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  base::WeakPtrFactory<SpellingMenuObserver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_SPELLING_MENU_OBSERVER_H_
