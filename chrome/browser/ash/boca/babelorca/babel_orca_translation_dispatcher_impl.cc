// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/babel_orca_translation_dispatcher_impl.h"

#include <memory>
#include <string>

#include "components/live_caption/translation_dispatcher.h"

namespace ash {

BabelOrcaTranslationDispatcherImpl::BabelOrcaTranslationDispatcherImpl(
    std::unique_ptr<::captions::TranslationDispatcher> translation_dispatcher)
    : translation_dispatcher_(std::move(translation_dispatcher)) {}

BabelOrcaTranslationDispatcherImpl::~BabelOrcaTranslationDispatcherImpl() =
    default;

void BabelOrcaTranslationDispatcherImpl::GetTranslation(
    const std::string& result,
    const std::string& source_language,
    const std::string& target_language,
    captions::OnTranslateEventCallback callback) {
  translation_dispatcher_->GetTranslation(result, source_language,
                                          target_language, std::move(callback));
}

}  // namespace ash
