// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/spelling_bubble_model.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/browser/spelling_service_client.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"

using content::BrowserThread;

const int kMaxSpellingSuggestions = 3;

SpellingMenuObserver::SpellingMenuObserver(RenderViewContextMenuProxy* proxy)
    : proxy_(proxy),
      loading_frame_(0),
      succeeded_(false),
      client_(new SpellingServiceClient) {
  if (proxy_ && proxy_->GetBrowserContext()) {
    Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
    integrate_spelling_service_.Init(
        spellcheck::prefs::kSpellCheckUseSpellingService, profile->GetPrefs());
  }
}

SpellingMenuObserver::~SpellingMenuObserver() {
}

void SpellingMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!params.misspelled_word.empty() ||
         params.dictionary_suggestions.empty());

  // Exit if we are not in an editable element because we add a menu item only
  // for editable elements.
  content::BrowserContext* browser_context = proxy_->GetBrowserContext();
  if (!params.is_editable || !browser_context)
    return;

  // Exit if there is no misspelled word.
  if (params.misspelled_word.empty())
    return;

  // Note that for Windows, suggestions_ will initially only contain those
  // suggestions obtained using Hunspell.
  suggestions_ = params.dictionary_suggestions;
  misspelled_word_ = params.misspelled_word;

  use_remote_suggestions_ = SpellingServiceClient::IsAvailable(
      browser_context, SpellingServiceClient::SUGGEST);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  use_platform_suggestions_ = spellcheck::UseBrowserSpellChecker();
  if (use_platform_suggestions_) {
    // Need to asynchronously retrieve suggestions from the platform
    // spellchecker, which requires the SpellcheckService.
    SpellcheckService* spellcheck_service =
        SpellcheckServiceFactory::GetForContext(browser_context);
    if (!spellcheck_service)
      return;

    // If there are no local suggestions or the spellcheck cloud service isn't
    // used, this separator will be removed later.
    proxy_->AddSeparator();

    // Append placeholders for maximum number of suggestions. Note that all but
    // the first placeholder will be made hidden in OnContextMenuShown override.
    // It can't be done here because UpdateMenuItem can only be called after
    // ToolkitDelegateViews is initialized, which happens after
    // RenderViewContextMenu::InitMenu.
    for (int i = 0;
         i < kMaxSpellingSuggestions &&
         IDC_SPELLCHECK_SUGGESTION_0 + i <= IDC_SPELLCHECK_SUGGESTION_LAST;
         ++i) {
      proxy_->AddMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + i,
                          /*title=*/std::u16string());
    }

    // Completion barrier for local (and possibly remote) retrieval of
    // suggestions. Remote suggestion cannot be displayed until local
    // suggestions have been retrieved, so that duplicates can be accounted for.
    completion_barrier_ = base::BarrierClosure(
        use_remote_suggestions_ ? 2 : 1,
        base::BindOnce(&SpellingMenuObserver::OnGetSuggestionsComplete,
                       weak_ptr_factory_.GetWeakPtr()));

    // Asynchronously retrieve suggestions from the platform spellchecker.
    spellcheck_platform::GetPerLanguageSuggestions(
        spellcheck_service->platform_spell_checker(), misspelled_word_,
        base::BindOnce(&SpellingMenuObserver::OnGetPlatformSuggestionsComplete,
                       weak_ptr_factory_.GetWeakPtr()));

    if (use_remote_suggestions_) {
      // Asynchronously retrieve remote suggestions in parallel.
      GetRemoteSuggestions();
    } else {
      // Animate first suggestion placeholder while retrieving suggestions.
      loading_message_ =
          l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_CHECKING);
      loading_frame_ = 0;
      animation_timer_.Start(
          FROM_HERE, base::Seconds(1),
          base::BindRepeating(&SpellingMenuObserver::OnAnimationTimerExpired,
                              weak_ptr_factory_.GetWeakPtr(),
                              IDC_SPELLCHECK_SUGGESTION_0));
    }

    // If there are no suggestions, this separator between suggestions and "Add
    // to dictionary" will be removed later.
    proxy_->AddSeparator();
  } else {
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    if (!suggestions_.empty() || use_remote_suggestions_)
      proxy_->AddSeparator();

    // Append Dictionary spell check suggestions.
    int length =
        std::min(kMaxSpellingSuggestions,
                 static_cast<int>(params.dictionary_suggestions.size()));
    for (int i = 0; i < length && IDC_SPELLCHECK_SUGGESTION_0 + i <=
                                      IDC_SPELLCHECK_SUGGESTION_LAST;
         ++i) {
      proxy_->AddMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + i,
                          params.dictionary_suggestions[i]);
    }

    if (use_remote_suggestions_)
      GetRemoteSuggestions();

    if (!params.dictionary_suggestions.empty()) {
      // |spellcheck_service| can be null when the suggested word is
      // provided by Web SpellCheck API.
      SpellcheckService* spellcheck_service =
          SpellcheckServiceFactory::GetForContext(browser_context);
      if (spellcheck_service && spellcheck_service->GetMetrics())
        spellcheck_service->GetMetrics()->RecordSuggestionStats(1);
      proxy_->AddSeparator();
    }
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  // If word is misspelled, give option for "Add to dictionary" and, if
  // multilingual spellchecking is not enabled, a check item "Ask Google for
  // suggestions".
  proxy_->AddMenuItem(
      IDC_SPELLCHECK_ADD_TO_DICTIONARY,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY));
  proxy_->AddSpellCheckServiceItem(integrate_spelling_service_.GetValue());
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellingMenuObserver::OnContextMenuShown(
    const content::ContextMenuParams& /*params*/,
    const gfx::Rect& /*bounds_in_screen*/) {
  if (!use_platform_suggestions_)
    return;

  // Disable the first place holder but keep it visible for animation if not
  // retrieving remote suggestions. Note that UpdateMenuItem does nothing if the
  // command_id is not found, e.g. if there is an early exit from InitMenu.
  proxy_->UpdateMenuItem(IDC_SPELLCHECK_SUGGESTION_0,
                         /*enabled=*/false,
                         /*hidden=*/use_remote_suggestions_, loading_message_);

  // Disable and hide the rest of the placeholders until suggestions obtained.
  for (int i = 1;
       i < kMaxSpellingSuggestions &&
       IDC_SPELLCHECK_SUGGESTION_0 + i <= IDC_SPELLCHECK_SUGGESTION_LAST;
       ++i) {
    proxy_->UpdateMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + i,
                           /*enabled=*/false, /*hidden=*/true,
                           /*title=*/std::u16string());
  }
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

bool SpellingMenuObserver::IsCommandIdSupported(int command_id) {
  if (command_id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      command_id <= IDC_SPELLCHECK_SUGGESTION_LAST)
    return true;

  switch (command_id) {
    case IDC_SPELLCHECK_ADD_TO_DICTIONARY:
    case IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS:
    case IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION:
    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
      return true;

    default:
      return false;
  }
}

bool SpellingMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());

  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_TOGGLE)
    return integrate_spelling_service_.GetValue() &&
           !profile->IsOffTheRecord();
  return false;
}

bool SpellingMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      command_id <= IDC_SPELLCHECK_SUGGESTION_LAST)
    return true;

  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  switch (command_id) {
    case IDC_SPELLCHECK_ADD_TO_DICTIONARY:
      return !misspelled_word_.empty();

    case IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS:
      return false;

    case IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION:
      return succeeded_;

    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
      return integrate_spelling_service_.IsUserModifiable() &&
             !profile->IsOffTheRecord();

    default:
      return false;
  }
}

void SpellingMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      command_id <= IDC_SPELLCHECK_SUGGESTION_LAST) {
    int suggestion_index = command_id - IDC_SPELLCHECK_SUGGESTION_0;
    proxy_->GetWebContents()->ReplaceMisspelling(
        suggestions_[suggestion_index]);
    // GetSpellCheckHost() can return null when the suggested word is provided
    // by Web SpellCheck API.
    content::BrowserContext* browser_context = proxy_->GetBrowserContext();
    if (browser_context) {
      SpellcheckService* spellcheck =
          SpellcheckServiceFactory::GetForContext(browser_context);
      if (spellcheck) {
        if (spellcheck->GetMetrics())
          spellcheck->GetMetrics()->RecordReplacedWordStats(1);
      }
    }
    return;
  }

  // When we choose the suggestion sent from the Spelling service, we replace
  // the misspelled word with the suggestion and add it to our custom-word
  // dictionary so this word is not marked as misspelled any longer.
  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION) {
    proxy_->GetWebContents()->ReplaceMisspelling(result_);
    misspelled_word_ = result_;
  }

  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION ||
      command_id == IDC_SPELLCHECK_ADD_TO_DICTIONARY) {
    // GetHostForProfile() can return null when the suggested word is provided
    // by Web SpellCheck API.
    content::BrowserContext* browser_context = proxy_->GetBrowserContext();
    if (browser_context) {
      SpellcheckService* spellcheck =
          SpellcheckServiceFactory::GetForContext(browser_context);
      if (spellcheck) {
        spellcheck->GetCustomDictionary()->AddWord(base::UTF16ToUTF8(
            misspelled_word_));

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
        if (spellcheck::UseBrowserSpellChecker()) {
          spellcheck_platform::AddWord(spellcheck->platform_spell_checker(),
                                       misspelled_word_);
        }
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)
      }
    }
  }

  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());

  // The spelling service can be toggled by the user only if it is not managed.
  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_TOGGLE &&
      integrate_spelling_service_.IsUserModifiable()) {
    bool spellcheckEnabled =
        profile &&
        profile->GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable);

    // When a user enables the "Use enhanced spell check" item, we check to see
    // if the user has spellcheck disabled. If the user has spellcheck disabled
    // but has already enabled the spelling service, we just enable spellcheck.
    // If spellcheck is enabled but the spelling service is not, we show a
    // bubble to confirm it. On the other hand, when a user disables this
    // item, we directly update/ the profile and stop integrating the spelling
    // service immediately.
    if (!spellcheckEnabled && integrate_spelling_service_.GetValue()) {
      if (profile) {
        profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable,
                                        true);
      }
    } else if (!integrate_spelling_service_.GetValue()) {
      // We use the web contents' primary main frame here rather than getting
      // the view from the local render frame host because we want to ensure
      // that it is non-null (proxy_->GetRenderFrameHost() can return nullptr if
      // the frame goes away). In these cases, the spelling preference changes
      // are still valid (tied to the BrowsingContext / WebContents) so we still
      // want to show the confirmation bubble.
      content::RenderFrameHost* rfh =
          proxy_->GetWebContents()->GetPrimaryMainFrame();
      gfx::Rect rect = rfh->GetRenderWidgetHost()->GetView()->GetViewBounds();
      std::unique_ptr<SpellingBubbleModel> model(
          new SpellingBubbleModel(profile, proxy_->GetWebContents()));
      chrome::ShowConfirmBubble(
          proxy_->GetWebContents()->GetTopLevelNativeWindow(),
          rfh->GetRenderWidgetHost()->GetView()->GetNativeView(),
          gfx::Point(rect.CenterPoint().x(), rect.y()), std::move(model));
    } else {
      if (profile) {
        profile->GetPrefs()->SetBoolean(
            spellcheck::prefs::kSpellCheckUseSpellingService, false);
      }
    }
  }
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellingMenuObserver::OnGetPlatformSuggestionsComplete(
    const spellcheck::PerLanguageSuggestions&
        platform_per_language_suggestions) {
  // Prioritize the platform results, since presumably the first user language
  // will have a Windows language pack installed. Treat the Hunspell suggestions
  // as if a single language.
  spellcheck::PerLanguageSuggestions per_language_suggestions =
      platform_per_language_suggestions;
  per_language_suggestions.push_back(suggestions_);

  std::vector<std::u16string> combined_suggestions;
  spellcheck::FillSuggestions(per_language_suggestions, &combined_suggestions);
  // suggestions_ will now include those from both the platform spellchecker and
  // Hunspell.
  suggestions_ = combined_suggestions;

  // The menu can be updated with local suggestions without waiting for the
  // request for remote suggestions to complete.
  if (suggestions_.empty() && !use_remote_suggestions_)
    proxy_->RemoveSeparatorBeforeMenuItem(IDC_SPELLCHECK_SUGGESTION_0);

  // Update spell check suggestions displayed on the menu.
  int length =
      std::min(kMaxSpellingSuggestions, static_cast<int>(suggestions_.size()));

  for (int i = 0; i < length && IDC_SPELLCHECK_SUGGESTION_0 + i <=
                                    IDC_SPELLCHECK_SUGGESTION_LAST;
       ++i) {
    proxy_->UpdateMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + i,
                           /*enabled=*/true, /*hidden=*/false, suggestions_[i]);
  }

  for (int i = suggestions_.size(); i < kMaxSpellingSuggestions; ++i) {
    // There were fewer suggestions returned than placeholder slots, remove the
    // empty menu items.
    proxy_->RemoveMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + i);
  }

  if (suggestions_.empty()) {
    proxy_->RemoveSeparatorBeforeMenuItem(IDC_SPELLCHECK_ADD_TO_DICTIONARY);
  } else {
    // |spellcheck_service| can be null when the suggested word is
    // provided by Web SpellCheck API.
    SpellcheckService* spellcheck_service =
        SpellcheckServiceFactory::GetForContext(proxy_->GetBrowserContext());
    if (spellcheck_service && spellcheck_service->GetMetrics())
      spellcheck_service->GetMetrics()->RecordSuggestionStats(1);
  }

  completion_barrier_.Run();
}

void SpellingMenuObserver::OnGetSuggestionsComplete() {
  animation_timer_.Stop();

  if (use_remote_suggestions_) {
    // Update remote suggestion too using cached values from
    // OnGetRemoteSuggestionsComplete.
    UpdateRemoteSuggestion(remote_service_type_, succeeded_, remote_results_);
  }

  FireSuggestionsCompleteCallbackIfNeededForTesting();
}

void SpellingMenuObserver::RegisterSuggestionsCompleteCallbackForTesting(
    base::OnceClosure callback) {
  suggestions_complete_callback_for_testing_ = std::move(callback);
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

void SpellingMenuObserver::GetRemoteSuggestions() {
  // The service types |SpellingServiceClient::SPELLCHECK| and
  // |SpellingServiceClient::SUGGEST| are mutually exclusive. Only one is
  // available at a time.
  //
  // When |SpellingServiceClient::SPELLCHECK| is available, the contextual
  // suggestions from |SpellingServiceClient| are already stored in
  // |params.dictionary_suggestions|.  |SpellingMenuObserver| places these
  // suggestions in the slots |IDC_SPELLCHECK_SUGGESTION_[0-LAST]|. If
  // |SpellingMenuObserver| queried |SpellingServiceClient| again, then
  // quality of suggestions would be reduced by lack of context around the
  // misspelled word.
  //
  // When |SpellingServiceClient::SUGGEST| is available,
  // |params.dictionary_suggestions| contains suggestions only from Hunspell
  // dictionary. |SpellingMenuObserver| queries |SpellingServiceClient| with
  // the misspelled word without the surrounding context. Spellcheck
  // suggestions from |SpellingServiceClient::SUGGEST| are not available until
  // |SpellingServiceClient| responds to the query. While
  // |SpellingMenuObserver| waits for |SpellingServiceClient|, it shows a
  // placeholder text "Loading suggestion..." in the
  // |IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION| slot. After
  // |SpellingServiceClient| responds to the query, |SpellingMenuObserver|
  // replaces the placeholder text with either the spelling suggestion or the
  // message "No more suggestions from Google." The "No more suggestions"
  // message is there when |SpellingServiceClient| returned the same
  // suggestion as Hunspell.
  //
  // Append a placeholder item for the suggestion from the Spelling service
  // and send a request to the service if we can retrieve suggestions from it.
  // Also, see if we can use the spelling service to get an ideal suggestion.
  // Otherwise, we'll fall back to the set of suggestions.  Initialize
  // variables used in OnTextCheckComplete(). We copy the input text to the
  // result text so we can replace its misspelled regions with suggestions.
  succeeded_ = false;
  result_ = misspelled_word_;

  // Add a placeholder item. This item will be updated when we receive a
  // response from the Spelling service. (We do not have to disable this
  // item now since Chrome will call IsCommandIdEnabled() and disable it.)
  loading_message_ =
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_CHECKING);
  proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION,
                      loading_message_);

  // Invoke a JSON-RPC call to the Spelling service in the background so we
  // can update the placeholder item when we receive its response. It also
  // starts the animation timer so we can show animation until we receive
  // it.
  content::BrowserContext* browser_context = proxy_->GetBrowserContext();
  if (!browser_context)
    return;
  bool result = client_->RequestTextCheck(
      browser_context, SpellingServiceClient::SUGGEST, misspelled_word_,
      base::BindOnce(&SpellingMenuObserver::OnGetRemoteSuggestionsComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     SpellingServiceClient::SUGGEST));
  if (result) {
    loading_frame_ = 0;
    animation_timer_.Start(
        FROM_HERE, base::Seconds(1),
        base::BindRepeating(&SpellingMenuObserver::OnAnimationTimerExpired,
                            weak_ptr_factory_.GetWeakPtr(),
                            IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION));
  }
}

void SpellingMenuObserver::OnGetRemoteSuggestionsComplete(
    SpellingServiceClient::ServiceType type,
    bool success,
    const std::u16string& /*text*/,
    const std::vector<SpellCheckResult>& results) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (use_platform_suggestions_) {
    // Cache results since we need the parallel retrieval of local suggestions
    // to also complete in order to proceed.
    remote_service_type_ = type;
    succeeded_ = success;
    // Parameter |text| is unused.
    remote_results_ = results;

    completion_barrier_.Run();
  } else {
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    animation_timer_.Stop();
    UpdateRemoteSuggestion(type, success, results);
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
}

void SpellingMenuObserver::UpdateRemoteSuggestion(
    SpellingServiceClient::ServiceType type,
    bool success,
    const std::vector<SpellCheckResult>& results) {
  // Scan the text-check results and replace the misspelled regions with
  // suggested words. If the replaced text is included in the suggestion list
  // provided by the local spellchecker, we show a "No suggestions from Google"
  // message.
  succeeded_ = success;
  if (results.empty()) {
    succeeded_ = false;
  } else {
    for (auto it = results.begin(); it != results.end(); ++it) {
      // If there's more than one replacement, we can't automatically apply it
      if (it->replacements.size() == 1)
        result_.replace(it->location, it->length, it->replacements[0]);
    }
    std::u16string result = base::i18n::ToLower(result_);
    for (std::vector<std::u16string>::const_iterator it = suggestions_.begin();
         it != suggestions_.end(); ++it) {
      if (result == base::i18n::ToLower(*it)) {
        succeeded_ = false;
        break;
      }
    }
  }
  if (type != SpellingServiceClient::SPELLCHECK) {
    if (!succeeded_) {
      result_ = l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_SPELLING_NO_SUGGESTIONS_FROM_GOOGLE);
    }

    // Update the menu item with the result text. We disable this item and hide
    // it when the spelling service does not provide valid suggestions.
    proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, succeeded_,
                           false, result_);
  }
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellingMenuObserver::FireSuggestionsCompleteCallbackIfNeededForTesting() {
  if (suggestions_complete_callback_for_testing_)
    std::move(suggestions_complete_callback_for_testing_).Run();
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

void SpellingMenuObserver::OnAnimationTimerExpired(int command_id) {
  // Append '.' characters to the end of "Checking".
  loading_frame_ = (loading_frame_ + 1) & 3;
  std::u16string loading_message =
      loading_message_ + std::u16string(loading_frame_, '.');

  // Update the menu item with the text. We disable this item to prevent users
  // from selecting it.
  proxy_->UpdateMenuItem(command_id,
                         /*enabled=*/false, /*hidden=*/false, loading_message);
}
