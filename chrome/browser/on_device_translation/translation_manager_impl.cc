// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_impl.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/translation_metrics.h"
#include "chrome/browser/on_device_translation/translator.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace on_device_translation {

namespace {

bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string& lang) {
  const std::string normalized_lang = l10n_util::GetLanguage(lang);
  return std::find_if(accept_languages.begin(), accept_languages.end(),
                      [&](const std::string_view& lang) {
                        return l10n_util::GetLanguage(lang) == normalized_lang;
                      }) != accept_languages.end();
}

bool IsSupportedPopularLanguage(const std::string& lang) {
  const std::optional<SupportedLanguage> supported_lang =
      ToSupportedLanguage(lang);
  if (!supported_lang) {
    return false;
  }
  return IsPopularLanguage(*supported_lang);
}

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(TranslationManagerImpl);

TranslationManagerImpl::TranslationManagerImpl(content::RenderFrameHost* rfh)
    : DocumentUserData<TranslationManagerImpl>(rfh) {
  browser_context_ = rfh->GetBrowserContext()->GetWeakPtr();
}

TranslationManagerImpl::~TranslationManagerImpl() = default;

// static
void TranslationManagerImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::TranslationManager> receiver) {
  TranslationManagerImpl* translation_manager =
      TranslationManagerImpl::GetOrCreateForCurrentDocument(render_frame_host);
  translation_manager->receiver_.Bind(std::move(receiver));
}

void TranslationManagerImpl::CanCreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    CanCreateTranslatorCallback callback) {
  CHECK(browser_context_);
  PrefService* profile_pref =
      Profile::FromBrowserContext(browser_context_.get())->GetPrefs();
  RecordTranslationAPICallForLanguagePair("CanTranslate", source_lang,
                                          target_lang);
  if (!profile_pref->GetBoolean(prefs::kTranslatorAPIAllowed)) {
    std::move(callback).Run(
        blink::mojom::CanCreateTranslatorResult::kNoDisallowedByPolicy);
    return;
  }
  if (!PassAcceptLanguagesCheck(
          profile_pref->GetString(language::prefs::kAcceptLanguages),
          source_lang, target_lang)) {
    std::move(callback).Run(
        blink::mojom::CanCreateTranslatorResult::kNoAcceptLanguagesCheckFailed);
    return;
  }
  OnDeviceTranslationServiceController::GetInstance()->CanTranslate(
      source_lang, target_lang, std::move(callback));
}

void TranslationManagerImpl::CreateTranslator(
    mojo::PendingRemote<blink::mojom::TranslationManagerCreateTranslatorClient>
        client,
    blink::mojom::TranslatorCreateOptionsPtr options) {
  RecordTranslationAPICallForLanguagePair("Create", options->source_lang,
                                          options->target_lang);
  CHECK(browser_context_);
  PrefService* profile_pref =
      Profile::FromBrowserContext(browser_context_.get())->GetPrefs();
  if (!profile_pref->GetBoolean(prefs::kTranslatorAPIAllowed)) {
    mojo::Remote(std::move(client))->OnResult(mojo::NullRemote());
    return;
  }
  if (!PassAcceptLanguagesCheck(
          profile_pref->GetString(language::prefs::kAcceptLanguages),
          options->source_lang, options->target_lang)) {
    mojo::Remote(std::move(client))->OnResult(mojo::NullRemote());
    return;
  }

  OnDeviceTranslationServiceController::GetInstance()->CreateTranslator(
      options->source_lang, options->target_lang,
      base::BindOnce(
          [](base::WeakPtr<TranslationManagerImpl> self,
             mojo::PendingRemote<
                 blink::mojom::TranslationManagerCreateTranslatorClient> client,
             const std::string& source_lang, const std::string& target_lang,
             mojo::PendingRemote<on_device_translation::mojom::Translator>
                 remote) {
            if (!client || !self) {
              // Request was aborted or the frame was destroyed. Note: Currently
              // aborting createTranslator() is not supported yet.
              // TODO(crbug.com/331735396): Support abort signal.
              return;
            }
            if (!remote) {
              mojo::Remote<
                  blink::mojom::TranslationManagerCreateTranslatorClient>(
                  std::move(client))
                  ->OnResult(mojo::NullRemote());
              return;
            }
            mojo::PendingRemote<::blink::mojom::Translator> blink_remote;
            self->translators_.Add(
                std::make_unique<Translator>(self->browser_context_,
                                             source_lang, target_lang,
                                             std::move(remote)),
                blink_remote.InitWithNewPipeAndPassReceiver());
            mojo::Remote<
                blink::mojom::TranslationManagerCreateTranslatorClient>(
                std::move(client))
                ->OnResult(std::move(blink_remote));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(client),
          options->source_lang, options->target_lang));
}

// static
bool TranslationManagerImpl::PassAcceptLanguagesCheck(
    const std::string& accept_languages_str,
    const std::string& source_lang,
    const std::string& target_lang) {
  if (!kTranslationAPIAcceptLanguagesCheck.Get()) {
    return true;
  }
  // When the TranslationAPIAcceptLanguagesCheck feature is enabled, the
  // Translation API will fail if neither the source nor destination language is
  // in the AcceptLanguages. This is intended to mitigate privacy concerns.
  const std::vector<std::string_view> accept_languages =
      base::SplitStringPiece(accept_languages_str, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  // TODO(crbug.com/371899260): Implement better language code handling.

  // One of the source or the destination language must be in the user's accept
  // language.
  const bool source_lang_is_in_accept_langs =
      IsInAcceptLanguage(accept_languages, source_lang);
  const bool target_lang_is_in_accept_langs =
      IsInAcceptLanguage(accept_languages, target_lang);
  if (!(source_lang_is_in_accept_langs || target_lang_is_in_accept_langs)) {
    return false;
  }

  // The other language must be a popular language.
  if (!source_lang_is_in_accept_langs &&
      !IsSupportedPopularLanguage(source_lang)) {
    return false;
  }
  if (!target_lang_is_in_accept_langs &&
      !IsSupportedPopularLanguage(target_lang)) {
    return false;
  }
  return true;
}

}  // namespace on_device_translation
