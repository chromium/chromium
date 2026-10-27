// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/chrome_omnibox_navigation_observer_android.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// static
void ChromeOmniboxNavigationObserverAndroid::Create(
    content::NavigationHandle* navigation_handle,
    Profile* profile,
    const std::u16string& omnibox_user_input,
    const AutocompleteMatch& selected_match,
    const AutocompleteMatch& alternative_nav_match) {
  if (!navigation_handle) {
    return;
  }

  // The observer will be kept alive until the navigation and the alternative
  // fetch (if any) finish.
  new ChromeOmniboxNavigationObserverAndroid(*navigation_handle, profile,
                                             omnibox_user_input, selected_match,
                                             alternative_nav_match);
}

ChromeOmniboxNavigationObserverAndroid::ChromeOmniboxNavigationObserverAndroid(
    content::NavigationHandle& navigation_handle,
    Profile* profile,
    const std::u16string& omnibox_user_input,
    const AutocompleteMatch& selected_match,
    const AutocompleteMatch& alternative_nav_match,
    network::mojom::URLLoaderFactory* loader_factory)
    : ChromeOmniboxNavigationObserverBase(navigation_handle,
                                          profile,
                                          omnibox_user_input,
                                          selected_match,
                                          alternative_nav_match,
                                          loader_factory) {}

ChromeOmniboxNavigationObserverAndroid::
    ~ChromeOmniboxNavigationObserverAndroid() {
  // Both the navigation and the alternative fetch (if any) hold a reference,
  // so both have finished by the time this runs.
  if (!web_contents()) {
    return;
  }
  if (fetch_state() == AlternativeFetchState::kFetchSucceeded) {
    ShowAlternativeNavMessage();
  }
}

void ChromeOmniboxNavigationObserverAndroid::ShowAlternativeNavMessage() {
  auto message = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::INTRANET_REDIRECT);
  message->SetTitle(l10n_util::GetStringFUTF16(
      IDS_INTRANET_REDIRECT_MESSAGE_TITLE,
      base::UTF8ToUTF16(alternative_nav_match().destination_url.spec())));
  message->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_INTRANET_REDIRECT_MESSAGE_PRIMARY_BUTTON));
  message->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFO_OUTLINE_LOGO_24DP));
  message->SetActionClick(base::BindOnce(
      [](base::WeakPtr<content::WebContents> web_contents,
         const std::u16string& text,
         const AutocompleteMatch& alternative_nav_match,
         const GURL& search_url) {
        OpenAlternativeNavMatch(web_contents.get(), text, alternative_nav_match,
                                search_url);
      },
      web_contents()->GetWeakPtr(), text(), alternative_nav_match(),
      match().destination_url));

  messages::MessageWrapper* message_ptr = message.get();
  if (messages::MessageDispatcherBridge::Get()->EnqueueMessage(
          message_ptr, web_contents(), messages::MessageScopeType::NAVIGATION,
          messages::MessagePriority::kNormal)) {
    message_ptr->SetDismissCallback(
        base::BindOnce([](std::unique_ptr<messages::MessageWrapper>,
                          messages::DismissReason) {},
                       std::move(message)));
  }
}
