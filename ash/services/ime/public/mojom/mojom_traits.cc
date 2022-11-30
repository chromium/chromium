// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/public/mojom/mojom_traits.h"

#include "ash/services/ime/public/mojom/input_method_host.mojom-shared.h"

namespace mojo {
namespace {

using CompletionCandidateDataView =
    ash::ime::mojom::CompletionCandidateDataView;
using SuggestionMode = ash::ime::mojom::SuggestionMode;
using SuggestionType = ash::ime::mojom::SuggestionType;
using SuggestionCandidateDataView =
    ash::ime::mojom::SuggestionCandidateDataView;
using TextCompletionCandidate = ash::ime::TextCompletionCandidate;
using TextSuggestionMode = ash::ime::TextSuggestionMode;
using TextSuggestionType = ash::ime::TextSuggestionType;
using TextSuggestion = ash::ime::TextSuggestion;

}  // namespace

SuggestionMode EnumTraits<SuggestionMode, TextSuggestionMode>::ToMojom(
    TextSuggestionMode mode) {
  switch (mode) {
    case TextSuggestionMode::kCompletion:
      return SuggestionMode::kCompletion;
    case TextSuggestionMode::kPrediction:
      return SuggestionMode::kPrediction;
  }
}

bool EnumTraits<SuggestionMode, TextSuggestionMode>::FromMojom(
    SuggestionMode input,
    TextSuggestionMode* output) {
  switch (input) {
    case SuggestionMode::kUnknown:
      // The browser process should never receive an unknown suggestion mode.
      // When adding a new SuggestionMode, the Chromium side should be updated
      // first to handle it, before changing the other calling side to send the
      // new suggestion mode.
      return false;
    case SuggestionMode::kCompletion:
      *output = TextSuggestionMode::kCompletion;
      return true;
    case SuggestionMode::kPrediction:
      *output = TextSuggestionMode::kPrediction;
      return true;
  }
}

SuggestionType EnumTraits<SuggestionType, TextSuggestionType>::ToMojom(
    TextSuggestionType type) {
  switch (type) {
    case TextSuggestionType::kAssistivePersonalInfo:
      return SuggestionType::kAssistivePersonalInfo;
    case TextSuggestionType::kAssistiveEmoji:
      return SuggestionType::kAssistiveEmoji;
    case TextSuggestionType::kMultiWord:
      return SuggestionType::kMultiWord;
  }
}

bool EnumTraits<SuggestionType, TextSuggestionType>::FromMojom(
    SuggestionType input,
    TextSuggestionType* output) {
  switch (input) {
    case SuggestionType::kUnknown:
      // The browser process should never receive an unknown suggestion type.
      // When adding a new SuggestionType, the Chromium side should be updated
      // first to handle it, before changing the other calling side to send the
      // new suggestion type.
      return false;
    case SuggestionType::kAssistivePersonalInfo:
      *output = TextSuggestionType::kAssistivePersonalInfo;
      return true;
    case SuggestionType::kAssistiveEmoji:
      *output = TextSuggestionType::kAssistiveEmoji;
      return true;
    case SuggestionType::kMultiWord:
      *output = TextSuggestionType::kMultiWord;
      return true;
  }
}

bool StructTraits<SuggestionCandidateDataView, TextSuggestion>::Read(
    SuggestionCandidateDataView input,
    TextSuggestion* output) {
  if (!input.ReadMode(&output->mode))
    return false;
  if (!input.ReadType(&output->type))
    return false;
  if (!input.ReadText(&output->text))
    return false;
  return true;
}

bool StructTraits<CompletionCandidateDataView, TextCompletionCandidate>::Read(
    CompletionCandidateDataView input,
    TextCompletionCandidate* output) {
  if (!input.ReadText(&output->text))
    return false;
  output->score = input.normalized_score();
  return true;
}

}  // namespace mojo
