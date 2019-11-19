// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
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
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
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

  suggestions_ = params.dictionary_suggestions;
  misspelled_word_ = params.misspelled_word;

  bool use_suggestions = SpellingServiceClient::IsAvailable(
      browser_context, SpellingServiceClient::SUGGEST);

  if (!suggestions_.empty() || use_suggestions)
    proxy_->AddSeparator();

  // Append Dictionary spell check suggestions.
  int length = std::min(kMaxSpellingSuggestions,
                        static_cast<int>(params.dictionary_suggestions.size()));
  for (int i = 0; i < length &&
       IDC_SPELLCHECK_SUGGESTION_0 + i <= IDC_SPELLCHECK_SUGGESTION_LAST;
       ++i) {
    proxy_->AddMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
                        params.dictionary_suggestions[i]);
  }

  // The service types |SpellingServiceClient::SPELLCHECK| and
  // |SpellingServiceClient::SUGGEST| are mutually exclusive. Only one is
  // available at at time.
  //
  // When |SpellingServiceClient::SPELLCHECK| is available, the contextual
  // suggestions from |SpellingServiceClient| are already stored in
  // |params.dictionary_suggestions|.  |SpellingMenuObserver| places these
  // suggestions in the slots |IDC_SPELLCHECK_SUGGESTION_[0-LAST]|. If
  // |SpellingMenuObserver| queried |SpellingServiceClient| again, then quality
  // of suggestions would be reduced by lack of context around the misspelled
  // word.
  //
  // When |SpellingServiceClient::SUGGEST| is available,
  // |params.dictionary_suggestions| contains suggestions only from Hunspell
  // dictionary. |SpellingMenuObserver| queries |SpellingServiceClient| with the
  // misspelled word without the surrounding context. Spellcheck suggestions
  // from |SpellingServiceClient::SUGGEST| are not available until
  // |SpellingServiceClient| responds to the query. While |SpellingMenuObserver|
  // waits for |SpellingServiceClient|, it shows a placeholder text "Loading
  // suggestion..." in the |IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION| slot. After
  // |SpellingServiceClient| responds to the query, |SpellingMenuObserver|
  // replaces the placeholder text with either the spelling suggestion or the
  // message "No more suggestions from Google." The "No more suggestions"
  // message is there when |SpellingServiceClient| returned the same suggestion
  // as Hunspell.
  if (use_suggestions) {
    // Append a placeholder item for the suggestion from the Spelling service
    // and send a request to the service if we can retrieve suggestions from it.
    // Also, see if we can use the spelling service to get an ideal suggestion.
    // Otherwise, we'll fall back to the set of suggestions.  Initialize
    // variables used in OnTextCheckComplete(). We copy the input text to the
    // result text so we can replace its misspelled regions with suggestions.
    succeeded_ = false;
    result_ = params.misspelled_word;

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
    bool result = client_->RequestTextCheck(
        browser_context,
        SpellingServiceClient::SUGGEST,
        params.misspelled_word,
        base::Bind(&SpellingMenuObserver::OnTextCheckComplete,
                   base::Unretained(this),
                   SpellingServiceClient::SUGGEST));
    if (result) {
      loading_frame_ = 0;
      animation_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1),
          this, &SpellingMenuObserver::OnAnimationTimerExpired);
    }
  }

  if (!params.dictionary_suggestions.empty()) {
    // |spellcheck_service| can be null when the suggested word is
    // provided by Web SpellCheck API.
    SpellcheckService* spellcheck_service =
        SpellcheckServiceFactory::GetForContext(browser_context);
    if (spellcheck_service && spellcheck_service->GetMetrics())
      spellcheck_service->GetMetrics()->RecordSuggestionStats(1);
    proxy_->AddSeparator();
  }

  // If word is misspelled, give option for "Add to dictionary" and, if
  // multilingual spellchecking is not enabled, a check item "Ask Google for
  // suggestions".
  proxy_->AddMenuItem(IDC_SPELLCHECK_ADD_TO_DICTIONARY,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY));
  proxy_->AddSpellCheckServiceItem(integrate_spelling_service_.GetValue());
}

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
      }
    }
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    if (spellcheck::UseBrowserSpellChecker()) {
      spellcheck_platform::AddWord(misspelled_word_);
    }
#endif
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
      content::RenderViewHost* rvh = proxy_->GetRenderViewHost();
      gfx::Rect rect = rvh->GetWidget()->GetView()->GetViewBounds();
      std::unique_ptr<SpellingBubbleModel> model(
          new SpellingBubbleModel(profile, proxy_->GetWebContents()));
      chrome::ShowConfirmBubble(
          proxy_->GetWebContents()->GetTopLevelNativeWindow(),
          rvh->GetWidget()->GetView()->GetNativeView(),
          gfx::Point(rect.CenterPoint().x(), rect.y()), std::move(model));
    } else {
      if (profile) {
        profile->GetPrefs()->SetBoolean(
            spellcheck::prefs::kSpellCheckUseSpellingService, false);
      }
    }
  }
}

void SpellingMenuObserver::OnTextCheckComplete(
    SpellingServiceClient::ServiceType type,
    bool success,
    const base::string16& text,
    const std::vector<SpellCheckResult>& results) {
  animation_timer_.Stop();

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
    base::string16 result = base::i18n::ToLower(result_);
    for (std::vector<base::string16>::const_iterator it = suggestions_.begin();
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

void SpellingMenuObserver::OnAnimationTimerExpired() {
  // Append '.' characters to the end of "Checking".
  loading_frame_ = (loading_frame_ + 1) & 3;
  base::string16 loading_message =
      loading_message_ + base::string16(loading_frame_,'.');

  // Update the menu item with the text. We disable this item to prevent users
  // from selecting it.
  proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, false, false,
                         loading_message);
}
